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

GB_CPU_Flags :: enum_flags u8 {
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
      F: GB_CPU_Flags;
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
  using,only(PC) cpu: GB_CPU;
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

  AF;
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
  vec_or_u3: int;
  op1: GB_Operand;
  op2: GB_Operand;
  op3: GB_Operand;

  #overlay (op1) op: [3] GB_Operand;
}

// NOTE: If we were going to make all of the operand comparisons at compile time anyway,
// there was really no reason why we had to do all this extra work on the Json.
gb_load_instruction_data :: () -> unprefixed: [] GB_Instruction, cbprefixed: [] GB_Instruction {
  Json_Opcode_Table :: struct #type_info_no_size_complaint {
    #insert -> string {
      #import "String";
      builder: String_Builder;
      for 0x0..0xFF {
        format := FormatInt.{base=16, value=it, minimum_digits=2};
        hex := sprint("%", format);
        to_upper_in_place(hex);
        print_to_builder(*builder, "_0x%: Json_Opcode_Data; @JsonName(0x%)\n", hex, hex);
      }

      return builder_to_string(*builder);
    }
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

  load_result, instrs := json_parse_file(OPCODE_DESCRIPTOR_PATH, Json_Instruction_Set);
  assert (load_result);
  unprefixed := NewArray(instrs.unprefixed.opcodes.count, GB_Instruction);
  cbprefixed := NewArray(instrs.cbprefixed.opcodes.count, GB_Instruction);
  instruction_sets := [] GB_Instruction.[unprefixed, cbprefixed];
  for set, set_index: instrs.set {
    for opcode, opcode_idx: set.opcodes {
      new_instr := *instruction_sets[set_index][opcode_idx];
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
            if (success && operand.name.count == 1) {
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

gb_generate_instruction_vtable :: ($unprefixed: [] GB_Instruction, $cbprefixed: [] GB_Instruction) -> unprefixed: [] gb_instr_proc, prefixed: [] gb_instr_proc {
  result_unprefixed := NewArray(unprefixed.count, gb_instr_proc);
  result_cbprefixed := NewArray(cbprefixed.count, gb_instr_proc);
  #insert -> string {
    prefix_sets := [] GB_Instruction.[unprefixed, cbprefixed];
    builder: String_Builder;
    for prefix: 0..1 {
      for prefix_sets[prefix] {
        print_to_builder(*builder, "%[%] = #procedure_of_call ", ifx prefix == 0 then "result_unprefixed" else "result_cbprefixed", it_index);
        if #complete it.mnemonic == {
          case .ADC; print_to_builder(*builder, "gb_instr_adc");
          case .ADD; print_to_builder(*builder, "gb_instr_add");
          case .AND; print_to_builder(*builder, "gb_instr_and");
          case .BIT; print_to_builder(*builder, "gb_instr_bit");
          case .CALL; print_to_builder(*builder, "gb_instr_call");
          case .CCF; print_to_builder(*builder, "gb_instr_ccf");
          case .CP; print_to_builder(*builder, "gb_instr_cp");
          case .CPL; print_to_builder(*builder, "gb_instr_cpl");
          case .DAA; print_to_builder(*builder, "gb_instr_daa");
          case .DEC; print_to_builder(*builder, "gb_instr_dec");
          case .DI; print_to_builder(*builder, "gb_instr_di");
          case .EI; print_to_builder(*builder, "gb_instr_ei");
          case .HALT; print_to_builder(*builder, "gb_instr_halt");
          case .INC; print_to_builder(*builder, "gb_instr_inc");
          case .JP; print_to_builder(*builder, "gb_instr_jp");
          case .JR; print_to_builder(*builder, "gb_instr_jr");
          case .LD; print_to_builder(*builder, "gb_instr_ld");
          case .LDH; print_to_builder(*builder, "gb_instr_ldh");
          case .NOP; print_to_builder(*builder, "gb_instr_nop");
          case .OR; print_to_builder(*builder, "gb_instr_or");
          case .POP; print_to_builder(*builder, "gb_instr_pop");
          case .PUSH; print_to_builder(*builder, "gb_instr_push");
          case .RES; print_to_builder(*builder, "gb_instr_res");
          case .RET; print_to_builder(*builder, "gb_instr_ret");
          case .RETI; print_to_builder(*builder, "gb_instr_reti");
          case .RL; print_to_builder(*builder, "gb_instr_rl");
          case .RLA; print_to_builder(*builder, "gb_instr_rla");
          case .RLC; print_to_builder(*builder, "gb_instr_rlc");
          case .RLCA; print_to_builder(*builder, "gb_instr_rlca");
          case .RR; print_to_builder(*builder, "gb_instr_rr");
          case .RRA; print_to_builder(*builder, "gb_instr_rra");
          case .RRC; print_to_builder(*builder, "gb_instr_rrc");
          case .RRCA; print_to_builder(*builder, "gb_instr_rrca");
          case .RST; print_to_builder(*builder, "gb_instr_rst");
          case .SBC; print_to_builder(*builder, "gb_instr_sbc");
          case .SCF; print_to_builder(*builder, "gb_instr_scf");
          case .SET; print_to_builder(*builder, "gb_instr_set");
          case .SLA; print_to_builder(*builder, "gb_instr_sla");
          case .SRA; print_to_builder(*builder, "gb_instr_sra");
          case .SRL; print_to_builder(*builder, "gb_instr_srl");
          case .STOP; print_to_builder(*builder, "gb_instr_stop");
          case .SUB; print_to_builder(*builder, "gb_instr_sub");
          case .SWAP; print_to_builder(*builder, "gb_instr_swap");
          case .XOR; print_to_builder(*builder, "gb_instr_xor");
          case .PREFIX; print_to_builder(*builder, "gb_instr_prefix");

          case .ILLEGAL_D3; #through;
          case .ILLEGAL_DB; #through;
          case .ILLEGAL_DD; #through;
          case .ILLEGAL_E3; #through;
          case .ILLEGAL_E4; #through;
          case .ILLEGAL_EB; #through;
          case .ILLEGAL_EC; #through;
          case .ILLEGAL_ED; #through;
          case .ILLEGAL_F4; #through;
          case .ILLEGAL_FC; #through;
          case .ILLEGAL_FD; print_to_builder(*builder, "gb_instr_nop");
        }
        instr_set := prefix_sets[prefix];
        op1 := instr_set[it_index].op1;
        op2 := instr_set[it_index].op2;
        op3 := instr_set[it_index].op3;
        vec_or_u3 := instr_set[it_index].vec_or_u3;
        print_to_builder(*builder, "(null, .%, .%, .%, %);\n", op1, op2, op3, vec_or_u3);
      }
    }

    return builder_to_string(*builder);
  }

  return result_unprefixed, result_cbprefixed;
}

// TODO: Horrible use of compile time, these should only be generated once and stored somewhere.
gb_unprefixed_instrs, gb_cbprefixed_instrs :: #run gb_load_instruction_data();
// NOTE: There may be a better way to do this, but so far this is the only setup where the compiler doesn't yell at me.
gb_instr_proc :: #type (gb: *GB_State);
gb_unprefixed_instr_vtbl, gb_cbprefixed_instr_vtbl :: #run,stallable gb_generate_instruction_vtable(gb_unprefixed_instrs, gb_cbprefixed_instrs);

gb_instr_adc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {
  #assert op1 == .A;
  carry := ifx (cpu.F & .CARRY) == .CARRY then 1 else 0;
  result: int = cpu.A + carry;

  #if op2 == {
    case .A; result += cpu.A;
    case .B; result += cpu.B;
    case .C; result += cpu.C;
    case .D; result += cpu.D;
    case .E; result += cpu.E;
    case .H; result += cpu.H;
    case .L; result += cpu.L;
    case .HL; result += base_memory_location[cpu.HL];
    case .n8; result += base_memory_location[PC+1];
    case; #assert false "Improper ADC implementation!";
  }

  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= ifx result > 0xFF then .CARRY;
  cpu.F |= ifx result > 0xF  then .HALF_CARRY;
  cpu.F &= ~.SUBTRACTION;

  cpu.A = xx result;
}

gb_instr_add :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {
  result: int;
  #if op1 == .A {
    result = cpu.A;
    #if op2 == {
      case .A; result += cpu.A;
      case .B; result += cpu.B;
      case .C; result += cpu.C;
      case .D; result += cpu.D;
      case .E; result += cpu.E;
      case .H; result += cpu.H;
      case .L; result += cpu.L;
      case .HL; result += base_memory_location[cpu.HL];
      case .n8; result += base_memory_location[PC+1];
      case; #assert false "Improper ADD implementation!";
    }
    cpu.F |= ifx result == 0 then .ZERO;
    cpu.F |= ifx result > 0xFF then .CARRY;
    cpu.F |= ifx result > 0xF  then .HALF_CARRY;
    cpu.A = xx result;
  } else #if op1 == .HL {
    result = cpu.HL;
    #if op2 == {
      case .HL; result += cpu.HL;
      case .BC; result += cpu.BC;
      case .DE; result += cpu.DE;
      case .SP; result += cpu.SP;
      case; #assert false "Improper ADD implementation!";
    }
    cpu.F |= ifx result > 0xFFFF then .CARRY;
    cpu.F |= ifx result > 0xFFF  then .HALF_CARRY;
    cpu.HL = xx result;
  } else #if op1 == .SP {
    #assert op2 == .e8 "Improper ADD implementation!";
    result = cpu.SP + cast(int) base_memory_location[PC+1];
    cpu.F |= ifx result > 0xFF then .CARRY;
    cpu.F |= ifx result > 0xF  then .HALF_CARRY;
    cpu.F &= ~.ZERO;
    cpu.SP = xx result;
  } else #assert false "Improper ADD implementation!";
  cpu.F &= ~.SUBTRACTION;
}

gb_instr_and :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {
  #assert op1 == .A;
  result := cpu.A;
  #if op2 == {
    case .A; result &= cpu.A;
    case .B; result &= cpu.B;
    case .C; result &= cpu.C;
    case .D; result &= cpu.D;
    case .E; result &= cpu.E;
    case .H; result &= cpu.H;
    case .L; result &= cpu.L;
    case .HL; result &= base_memory_location[cpu.HL];
    case .n8; result &= base_memory_location[PC+1];
    case; #assert false "Improper ADD implementation!";
  }

  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= .HALF_CARRY;
  cpu.F &= ~.SUBTRACTION;
  cpu.F &= ~.CARRY;
  cpu.A = result;
}

gb_instr_bit :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_call :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_ccf :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_cp :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_cpl :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_daa :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_dec :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_di :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_ei :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_halt :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_inc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_jp :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_jr :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_ld :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_ldh :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_nop :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_or :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_pop :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_push :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_res :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_ret :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_reti :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_rl :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_rla :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_rlc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_rlca :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_rr :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_rra :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_rrc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_rrca :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_rst :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_sbc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_scf :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_set :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_sla :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_sra :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_srl :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_stop :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_sub :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_swap :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_xor :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}

gb_instr_prefix :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $op3: GB_Operand, $vec_or_u3: int) {

}


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

  /* TODO: This needs to be mapped into GB address space
  test_rom, rom_load_result := read_entire_file(TEST_ROM_PATH);
  assert (rom_load_result);
  advance(*test_rom, ROM_START);
  for opcode: test_rom {
    //instr_data := instrs.unprefixed.opcodes[opcode];

    //it_index += instr_data.bytes - 1;
  }
  */
  ROM_START :: 0x0100;
  test_rom, rom_load_result := read_entire_file(TEST_ROM_PATH);
  assert(rom_load_result);
  memcpy(gb_state.base_memory_location, test_rom.data, test_rom.count);

  run_emulator := true;
  while run_emulator {

  }
}