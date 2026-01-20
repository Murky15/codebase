#import "Basic";
#import "File";
#import "Json";
#import "Flat_Pool";
#import "Reflection";

// TODO: Until we make a proper first.jai
#if OS == .WINDOWS {
  TEST_ROM_PATH :: "W:/code/soniboy/test_roms/cpu_instrs.gb";
  OPCODE_DESCRIPTOR_PATH :: "W:/code/soniboy/gb_opcodes.json";
} else #if OS == .LINUX {
  TEST_ROM_PATH :: "/home/gmb/Workspace/root/code/soniboy/test_roms/cpu_instrs.gb";
  OPCODE_DESCRIPTOR_PATH :: "/home/gmb/Workspace/root/code/soniboy/gb_opcodes.json";
}

GB_CPU_Flags :: enum_flags {
  ZERO :: 0x80;
  SUBTRACTION :: 0x40;
  HALF_CARRY :: 0x20;
  CARRY :: 0x10;
}

GB_CPU :: struct {
  union {
    AF: u16;
    struct {
      A: u8;
      F: u8;
    }
  }
  union {
    BC: u16;
    struct {
      B: u8;
      C: u8;
    }
  }
  union {
    DE: u16;
    struct {
      D: u8;
      E: u8;
    }
  }
  union {
    HL: u16;
    struct {
      H: u8;
      L: u8;
    }
  }
  SP: u16;
  PC: u16;
}

GB_State :: struct {
  cpu: GB_CPU;
  base_memory_location: *u8;
}

GB_Mnemonic :: enum {
  ADC;
  ADD;
  AND;
  BIT;
  CALL;
  CCF;
  CP;
  CPL;
  DAA;
  DEC;
  DI;
  EI;
  HALT;
  INC;
  JP;
  JR;
  LD;
  LDH;
  NOP;
  OR;
  POP;
  PUSH;
  RES;
  RET;
  RETI;
  RL;
  RLA;
  RLC;
  RLCA;
  RR;
  RRA;
  RRC;
  RRCA;
  RST;
  SBC;
  SCF;
  SET;
  SLA;
  SRA;
  SRL;
  STOP;
  SUB;
  SWAP;
  XOR;
  PREFIX;

  ILLEGAL_D3;
  ILLEGAL_DB;
  ILLEGAL_DD;
  ILLEGAL_E3;
  ILLEGAL_E4;
  ILLEGAL_EB;
  ILLEGAL_EC;
  ILLEGAL_ED;
  ILLEGAL_F4;
  ILLEGAL_FC;
  ILLEGAL_FD;
}

GB_Operand :: enum {
  NONE;

  A;
  B;
  C;
  D;
  E;
  H;
  L;

  BC;
  DE;
  HL;

  SP;
  PC;

  n8;
  n16;
  a8;
  a16;
  e8;
  u3;

  Z;
  NZ;
  NC;

  VEC;
}

GB_Instruction :: struct {
  mnemonic: GB_Mnemonic;
  size: int;
  op1: GB_Operand;
  op2: GB_Operand;
  op3: GB_Operand;
  vec_or_u3: int;

  #overlay (op1) op: [3] GB_Operand;
}

gb_load_instruction_data :: () -> unprefixed: [] GB_Instruction, cbprefixed: [] GB_Instruction {
  Json_Opcode_Table :: struct #type_info_no_size_complaint {
    #insert #run () -> string {
      #import "String";
      builder: String_Builder;
      for 0x0..0xFF {
        format := FormatInt.{base=16, value=it, minimum_digits=2};
        hex := sprint("%", format);
        to_upper_in_place(hex);
        print_to_builder(*builder, "_0x%: Json_Opcode_Data; @JsonName(0x%)\n", hex, hex);
      }

      return builder_to_string(*builder);
    }();
    #overlay (_0x00) opcodes: [0x100] Json_Opcode_Data;

    Json_Opcode_Data :: struct {
      mnemonic: string;
      bytes: int;
      cycles: [] int;
      operands: [] Json_Operand;
      immediate: bool;
      flags: *JSON_Value;

      Json_Operand :: struct {
        name: string;
        immediate: bool;
        increment: bool;
        bytes: int;
      }
    }
  }

  Json_Instruction_Set :: struct {
    unprefixed: Json_Opcode_Table;
    cbprefixed: Json_Opcode_Table;

    #overlay (unprefixed) set: [2] Json_Opcode_Table;
  };

  load_result, instrs := json_parse_file(OPCODE_DESCRIPTOR_PATH, Json_Instruction_Set,, temp);
  assert (load_result);
  unprefixed := NewArray(instrs.unprefixed.opcodes.count, GB_Instruction);
  cbprefixed := NewArray(instrs.cbprefixed.opcodes.count, GB_Instruction);
  instruction_sets := [] GB_Instruction.[unprefixed, cbprefixed];
  for set, set_index: instrs.set {
    for opcode: set.opcodes {
      new_instr := *instruction_sets[set_index][it_index];
      new_instr.size = opcode.bytes;
      m_info := type_info(GB_Mnemonic);
      found_mnemonic := false;
      for m_info.names {
        if it == opcode.mnemonic {
          found_mnemonic = true;
          new_instr.mnemonic = cast(GB_Mnemonic) m_info.values[it_index];
        }
      }
      if !found_mnemonic {
        assert(false, "Unrecognized instruction mnemonic: %", opcode.mnemonic);
      }
      op_info := type_info(GB_Operand);
      for operand, operand_idx: opcode.operands {
        found_operand := false;
        for op_info.names {
          if it == operand.name {
            found_operand = true;
            new_instr.op[operand_idx] = cast(GB_Operand) op_info.values[it_index];
          }
        }
        if !found_operand {
          // NOTE: We are probably dealing with an RST vector (0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, and 0x38),
          // or an unsigned 3-bit integer (u3).
          val, success, _ := string_to_int(advance(operand.name, 1), 16);
          if (success) {
            new_instr.op[operand_idx] = .VEC;
            new_instr.vec_or_u3 = val;
          } else {
            val, success, _ = string_to_int(operand.name);
            if (success) {
              new_instr.op[operand_idx] = .u3;
              new_instr.vec_or_u3 = val;
            } else {
              assert(false, "Unrecognized instruction operand: %", operand.name);
            }
          }
        }
      }
    }
  }

  return unprefixed, cbprefixed;
}

/*
  Here are my options for instructions:
  A. Give all mnemonics a dedicated function with a uniform signature. Store in Vtable. Easy lookup, worst performance.
  B. Create tailored mnemonic functions with auto-baked parameters. Must be hardcoded. Painful to write, best performance.
*/

main :: () {
  perm_arena: Flat_Pool;
  context.allocator.proc = flat_pool_allocator_proc;
  context.allocator.data = *perm_arena;
  default_format_int := context.print_style.default_format_int;
  defer context.print_style.default_format_int = default_format_int;
  format_int := *context.print_style.default_format_int;
  format_int.base = 16;

  gb_state: GB_State;
  gb_state.base_memory_location = cast(*u8) alloc(0xFFFF);
  gb_unprefixed_instrs, gb_cbprefixed_instrs := gb_load_instruction_data();

  ROM_START :: 0x0100;
  test_rom, rom_load_result := read_entire_file(TEST_ROM_PATH);
  assert (rom_load_result);
  advance(*test_rom, ROM_START);
  for opcode: test_rom {
    //instr_data := instrs.unprefixed.opcodes[opcode];

    //it_index += instr_data.bytes - 1;
  }
}