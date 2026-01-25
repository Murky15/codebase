#import "Basic";
#import "File";
#import "Json";
#import "Flat_Pool";
#import "Reflection";

/*
TODO:
- first.jai
- Propper logging
*/

// TODO: Until we make a proper first.jai
#if OS == .WINDOWS {
  TEST_ROM_PATH :: "W:/code/soniboy/test_roms/cpu_instrs.gb";
  OPCODE_DESCRIPTOR_PATH :: "W:/code/soniboy/gb_opcodes.json";
} else #if OS == .LINUX {
  TEST_ROM_PATH :: "/home/gmb/Workspace/root/code/soniboy/test_roms/01-special.gb";
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
  IME: bool;
}

GB_State :: struct {
  cpu: GB_CPU;
  base_mem: *u8;
}

GB_Instruction :: struct {
  mnemonic: GB_Mnemonic;
  size: u16;
  vec_or_u3: u8;
  op1: GB_Operand;
  op2: GB_Operand;

  #overlay (op1) op: [2] GB_Operand;
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
  SPI;
  SP_e8;
  PC;

  n8;
  n16;
  e8;

  a8;
  a16;
  CIND;
  BCIND;
  DEIND;
  HLIND;
  HLI;
  HLD;
  a8IND;
  a16IND;

  u3;
  VEC;

  Z;
  NZ;
  NC;
}

gb_pun_u8_s8 :: (val: u8) -> s8 #expand {
  punner: union {
    u: u8;
    s: s8;
  };
  punner.u = val;

  return punner.s;
}

gb_operand_is_r8 :: (op: GB_Operand) -> bool #expand {
  return op <= .L;
}

gb_operand_is_r16 :: (op: GB_Operand) -> bool #expand {
  return op >= .AF && op <= .PC;
}

gb_operand_is_num :: (op: GB_Operand) -> bool #expand {
  return op >= .n8 && op <= .e8;
}

gb_operand_is_address_or_indirect :: (op: GB_Operand) -> bool #expand {
  return op >= .a8 && op <= .a16IND;
}

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
        decrement: bool;
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
      new_instr.size = cast(u16) opcode.bytes;
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

      if opcode.operands.count > 2 {
        assert(new_instr.mnemonic == .LD); // There should only be one...
        new_instr.op1 = .HL;
        new_instr.op2 = .SP_e8;
        continue;
      }

      for * operand, operand_idx: opcode.operands {
        found_operand := false;
        special_operand := false;
        if operand.increment {
          operand.name = sprint("%%", operand.name, "I");
          special_operand = true;
        } else if operand.decrement {
          operand.name = sprint("%%", operand.name, "D");
          special_operand = true;
        } else if !operand.immediate {
          operand.name = sprint("%%", operand.name, "IND");
          special_operand = true;
        }
        for op_info.names {
          if it == operand.name {
            found_operand = true;
            new_instr.op[operand_idx] = cast(GB_Operand) op_info.values[it_index];
          }
        }
        if !found_operand && special_operand then assert(false, "Unrecognized instruction operand: %", operand.name);
        if !found_operand {
          // NOTE: We are probably dealing with an RST vector (0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, and 0x38),
          // or an unsigned 3-bit integer (u3).
          val, success, _ := string_to_int(advance(operand.name, 1), 16);
          if (success) {
            assert(operand.immediate, "Unhandled special operand: % with opcode: %\n", operand.*, opcode_idx);
            new_instr.op[operand_idx] = .VEC;
            new_instr.vec_or_u3 = cast(u8) val;
          } else {
            val, success, _ = string_to_int(operand.name);
            if (success && operand.name.count == 1) {
              new_instr.op[operand_idx] = .u3;
              new_instr.vec_or_u3 = cast(u8) val;
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
        vec_or_u3 := instr_set[it_index].vec_or_u3;
        print_to_builder(*builder, "(null, .%, .%, %);\n", op1, op2, vec_or_u3);
      }
    }

    return builder_to_string(*builder);
  }

  return result_unprefixed, result_cbprefixed;
}

// NOTE: There may be a better way to do this, but so far this is the only setup where the compiler doesn't yell at me.
gb_instr_proc :: #type (gb: *GB_State);

gb_instr_adc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #assert op1 == .A; "Improper ADC implementation!";
  carry := ifx cast(bool, cpu.F & .CARRY) then 1 else 0;
  result: u64 = cpu.A + cast(u64) carry;

  #if op2 == {
    case .A; result += cpu.A;
    case .B; result += cpu.B;
    case .C; result += cpu.C;
    case .D; result += cpu.D;
    case .E; result += cpu.E;
    case .H; result += cpu.H;
    case .L; result += cpu.L;
    case .HLIND; result += base_mem[cpu.HL];
    case .n8; result += base_mem[cpu.PC+1];
    case; #assert false "Improper ADC implementation!";
  }

  cpu.F = 0;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= ifx result > 0xFF then .CARRY;
  cpu.F |= ifx result > 0xF  then .HALF_CARRY;

  cpu.A = xx result;
}

gb_instr_add :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  result: u64;
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
      case .HLIND; result += base_mem[cpu.HL];
      case .n8; result += base_mem[cpu.PC+1];
      case; #assert false "Improper ADD implementation!";
    }
    cpu.F = 0;
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
    cpu.F &= ~.SUBTRACTION;
    cpu.F |= ifx result > 0xFFFF then .CARRY;
    cpu.F |= ifx result > 0xFFF  then .HALF_CARRY;
    cpu.HL = xx result;
  } else #if op1 == .SP {
    #assert op2 == .e8 "Improper ADD implementation!";
    s := gb_pun_u8_s8(base_mem[cpu.PC+1]);
    result = cast(u64, cast(s16) cpu.SP + s);
    cpu.F = 0;
    cpu.F |= ifx result > 0xFF then .CARRY;
    cpu.F |= ifx result > 0xF  then .HALF_CARRY;
    cpu.SP = xx result;
  } else #assert false "Improper ADD implementation!";
}

gb_instr_and :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #assert op1 == .A; "Improper AND implementation!";
  result := cpu.A;
  #if op2 == {
    case .A; result &= cpu.A;
    case .B; result &= cpu.B;
    case .C; result &= cpu.C;
    case .D; result &= cpu.D;
    case .E; result &= cpu.E;
    case .H; result &= cpu.H;
    case .L; result &= cpu.L;
    case .HLIND; result &= base_mem[cpu.HL];
    case .n8; result &= base_mem[cpu.PC+1];
    case; #assert false "Improper AND implementation!";
  }

  cpu.F = 0;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= .HALF_CARRY;
  cpu.A = result;
}

gb_instr_bit :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #assert op1 == .u3; "Improper BIT implementation!";
  bit_set: bool;
  #if op2 == {
    case .A; bit_set = cast(bool, (cpu.A >> vec_or_u3) & 0x1);
    case .B; bit_set = cast(bool, (cpu.B >> vec_or_u3) & 0x1);
    case .C; bit_set = cast(bool, (cpu.C >> vec_or_u3) & 0x1);
    case .D; bit_set = cast(bool, (cpu.D >> vec_or_u3) & 0x1);
    case .E; bit_set = cast(bool, (cpu.E >> vec_or_u3) & 0x1);
    case .H; bit_set = cast(bool, (cpu.H >> vec_or_u3) & 0x1);
    case .L; bit_set = cast(bool, (cpu.L >> vec_or_u3) & 0x1);
    case .HLIND; bit_set = cast(bool, (base_mem[cpu.HL] >> vec_or_u3) & 0x1);
    case; #assert false "Improper BIT implementation!";
  }

  cpu.F &= ~.ZERO;
  cpu.F &= ~.SUBTRACTION;
  cpu.F |= ifx !bit_set then .ZERO;
  cpu.F |= .HALF_CARRY;
}

gb_instr_call :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  carry_set := cast(bool, cpu.F & .CARRY);
  zero_set := cast(bool, cpu.F & .ZERO);
  should_call := true;
  #if op1 == {
    case .a16;

    case .C;  if !carry_set then should_call = false;
    case .NC; if  carry_set then should_call = false;
    case .Z;  if !zero_set  then should_call = false;
    case .NZ; if  zero_set  then should_call = false;
    case; #assert false "Improper CALL implementation";
  }
  if should_call {
    next_instr := cpu.PC + 3; // NOTE: Instruction is 3 bytes long
    base_mem[cpu.SP - 2] = cast(u8, next_instr & 0xFF);
    base_mem[cpu.SP - 1] = cast(u8, next_instr >> 8);
    cpu.SP -= 2;
    gb_instr_jp(gb, .a16, .NONE, 0);
  }
}

gb_instr_ccf :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  cpu.F &= ~.SUBTRACTION;
  cpu.F &= ~.HALF_CARRY;
  cpu.F ^= .CARRY;
}

gb_instr_cp :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #assert op1 == .A; "Improper CP implementation!";
  result: u64 = cpu.A;
  val: u64;

  #if op2 == {
    case .A; val = cpu.A;
    case .B; val = cpu.B;
    case .C; val = cpu.C;
    case .D; val = cpu.D;
    case .E; val = cpu.E;
    case .H; val = cpu.H;
    case .L; val = cpu.L;
    case .HLIND; val = base_mem[cpu.HL];
    case .n8; val = base_mem[cpu.PC+1];
    case; #assert false "Improper CP implementation!";
  }
  result -= val;

  cpu.F = 0;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= .SUBTRACTION;
  cpu.F |= ifx val > cpu.A then .CARRY;
  cpu.F |= ifx val & 0xF > cpu.A & 0xF then .HALF_CARRY;
}

gb_instr_cpl :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  cpu.A = ~cpu.A;
  cpu.F |= .SUBTRACTION;
  cpu.F |= .HALF_CARRY;
}

gb_instr_daa :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  result: u64;
  if cpu.F & .SUBTRACTION {
    adjustment: int;
    adjustment += ifx cpu.F & .HALF_CARRY then 0x6;
    adjustment += ifx cpu.F & .CARRY then 0x60;
    result = cpu.A - cast(u64) adjustment;
  } else {
    adjustment: int;
    adjustment += ifx cpu.F & .HALF_CARRY || cpu.A & 0xF > 0x9 then 0x6;
    adjustment += ifx cpu.F & .CARRY || cpu.A > 0x99 {
      cpu.F |= .CARRY;
      0x60;
    }
    result = cpu.A + cast(u64) adjustment;
  }

  cpu.F &= ~.ZERO;
  cpu.F &= ~.HALF_CARRY;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.A = xx result;
}

gb_instr_dec :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  result: u64;
  prev_value: u64;
  #if op1 == {
    case .A; result = cpu.A - 1; prev_value = cpu.A; cpu.A = xx result;
    case .B; result = cpu.B - 1; prev_value = cpu.B; cpu.B = xx result;
    case .C; result = cpu.C - 1; prev_value = cpu.C; cpu.C = xx result;
    case .D; result = cpu.D - 1; prev_value = cpu.D; cpu.D = xx result;
    case .E; result = cpu.E - 1; prev_value = cpu.E; cpu.E = xx result;
    case .H; result = cpu.H - 1; prev_value = cpu.H; cpu.H = xx result;
    case .L; result = cpu.L - 1; prev_value = cpu.L; cpu.L = xx result;
    case .HLIND; {
      result = base_mem[cpu.HL] - 1;
      prev_value = base_mem[cpu.HL];
      base_mem[cpu.HL] = xx result;
    }

    case .AF; cpu.AF -= 1;
    case .BC; cpu.BC -= 1;
    case .DE; cpu.DE -= 1;
    case .HL; cpu.HL -= 1;

    case .SP; cpu.SP -= 1;
    case; #assert false "Improper DEC implementation!";
  }

  #if #run gb_operand_is_r8(op1) || op1 == .HLIND {
    cpu.F &= ~.ZERO;
    cpu.F &= ~.HALF_CARRY;
    cpu.F |= ifx result == 0 then .ZERO;
    cpu.F |= ifx result & 0xF > prev_value & 0xF then .HALF_CARRY;
  }
}

gb_instr_di :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  cpu.IME = false;
}

gb_instr_ei :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  // TODO: How do I delay this to the instruction following the one *after* this?
  // TODO: Also we can make custom loggers in this language with an actual prefix
  // NOTE: I think the best way to handle this would be to check for it in the main fetch/execute loop and
  // handle accordingly, probably with a loop counter.
  cpu.IME = true;
  log("GB: EI instruction is not correctly implemented!\n", flags=.WARNING);
}

gb_instr_halt :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {

}

gb_instr_inc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  result: u64;
  prev_value: u64;
  #if op1 == {
    case .A; result = cpu.A + 1; prev_value = cpu.A; cpu.A = xx result;
    case .B; result = cpu.B + 1; prev_value = cpu.B; cpu.B = xx result;
    case .C; result = cpu.C + 1; prev_value = cpu.C; cpu.C = xx result;
    case .D; result = cpu.D + 1; prev_value = cpu.D; cpu.D = xx result;
    case .E; result = cpu.E + 1; prev_value = cpu.E; cpu.E = xx result;
    case .H; result = cpu.H + 1; prev_value = cpu.H; cpu.H = xx result;
    case .L; result = cpu.L + 1; prev_value = cpu.L; cpu.L = xx result;
    case .HLIND; {
      result = base_mem[cpu.HL] + 1;
      prev_value = base_mem[cpu.HL];
      base_mem[cpu.HL] = xx result;
    }

    case .AF; cpu.AF += 1;
    case .BC; cpu.BC += 1;
    case .DE; cpu.DE += 1;
    case .HL; cpu.HL += 1;

    case .SP; cpu.SP += 1;
    case; #assert false "Improper INC implementation!";
  }

  #if #run gb_operand_is_r8(op1) || op1 == .HLIND {
    cpu.F &= ~.ZERO;
    cpu.F &= ~.HALF_CARRY;
    cpu.F &= ~.SUBTRACTION;
    cpu.F |= ifx result == 0  then .ZERO;
    cpu.F |= ifx result > 0xF then .HALF_CARRY;
  }
}

gb_instr_jp :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  address := #ifx op1 == .a16 || op2 == .a16 {
    cast(u16, base_mem[cpu.PC+2] << 8 | base_mem[cpu.PC+1]);
  } else {
    cpu.HL;
  }

  carry_set := cast(bool, cpu.F & .CARRY);
  zero_set := cast(bool, cpu.F & .ZERO);
  should_jump := true;
  #if op1 == {
    case .a16;
    case .HL;

    case .C;  if !carry_set then should_jump = false;
    case .NC; if  carry_set then should_jump = false;
    case .Z;  if !zero_set  then should_jump = false;
    case .NZ; if  zero_set  then should_jump = false;
    case; #assert false "Improper JP implementation";
  }

  if should_jump then cpu.PC = address;
}

gb_instr_jr :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  offset := gb_pun_u8_s8(base_mem[cpu.PC+1]);

  carry_set := cast(bool, cpu.F & .CARRY);
  zero_set := cast(bool, cpu.F & .ZERO);
  should_jump := true;
  #if op1 == {
    case .e8;

    case .C;  if !carry_set then should_jump = false;
    case .NC; if  carry_set then should_jump = false;
    case .Z;  if !zero_set  then should_jump = false;
    case .NZ; if  zero_set  then should_jump = false;
    case; #assert false "Improper JR implementation";
  }

  if should_jump {
    new_addr := cast(int, cpu.PC + 1) + offset;
    cpu.PC = xx new_addr;
  }
}

gb_instr_ld :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  src_type :: #ifx #run (gb_operand_is_r8(op1) || gb_operand_is_address_or_indirect(op1)) then u8 else u16;
  dest_type :: *src_type;
  dest: dest_type;
  src: src_type;
  #if op1 == {
    case .A; dest = *cpu.A;
    case .B; dest = *cpu.B;
    case .C; dest = *cpu.C;
    case .D; dest = *cpu.D;
    case .E; dest = *cpu.E;
    case .H; dest = *cpu.H;
    case .L; dest = *cpu.L;
    case .BC; dest = *cpu.BC;
    case .DE; dest = *cpu.DE;
    case .HL; dest = *cpu.HL;
    case .SP; dest = *cpu.SP;

    case .BCIND; dest = base_mem + cpu.BC;
    case .DEIND; dest = base_mem + cpu.DE;
    case .HLIND; dest = base_mem + cpu.HL;
    case .HLI; dest = base_mem + cpu.HL; cpu.HL += 1;
    case .HLD; dest = base_mem + cpu.HL; cpu.HL -= 1;
    case .a16IND; {
      address := cast(u16, base_mem[cpu.PC+2] << 8 | base_mem[cpu.PC+1]);
      dest = base_mem + address;
    }

    case; #assert false "Improper LD Implementation!";
  }

  #if op2 == {
    case .A; src = cpu.A;
    case .B; src = cpu.B;
    case .C; src = cpu.C;
    case .D; src = cpu.D;
    case .E; src = cpu.E;
    case .H; src = cpu.H;
    case .L; src = cpu.L;
    case .HL; src = cpu.HL;
    case .SP; {
      #assert op1 == .a16IND "Improper LD Implementation!";
      dest[0] = cast(u8, cpu.SP & 0xFF);
      dest[1] = cast(u8, cpu.SP >> 8);
      return;
    }

    case .n8; src = base_mem[cpu.PC+1];
    case .n16; src = base_mem[cpu.PC+2] << 8 | base_mem[cpu.PC+1];

    case .HLIND; src = base_mem[cpu.HL];
    case .HLI; src = base_mem[cpu.HL]; cpu.HL += 1;
    case .HLD; src = base_mem[cpu.HL]; cpu.HL -= 1;
    case .BCIND; src = base_mem[cpu.BC];
    case .DEIND; src = base_mem[cpu.DE];
    case .a16IND; {
      address := cast(u16, base_mem[cpu.PC+2] << 8 | base_mem[cpu.PC+1]);
      src = base_mem[address];
    }

    case .SP_e8; {
      s := gb_pun_u8_s8(base_mem[cpu.PC+1]);
      rel_addr := cast(int) cpu.SP + s;
      src = xx rel_addr;

      cpu.F = 0;
      cpu.F |= ifx rel_addr > 0xFF then .CARRY;
      cpu.F |= ifx rel_addr > 0xF  then .HALF_CARRY;
    }

    case; #assert false "Improper LD Implementation!";
  }

  dest.* = src;
}

gb_instr_ldh :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  dest: *u8;
  src: u8;
  #if op1 == {
    case .A; dest = *cpu.A;
    case .CIND; dest = base_mem + 0xFF00 + cpu.C;
    case .a8IND; dest = base_mem + 0xFF00 + base_mem[cpu.PC+1];
    case; #assert false "Improper LDH Implementation!";
  }

  #if op2 == {
    case .A; src = cpu.A;
    case .CIND; src = base_mem[0xFF00 + cpu.C];
    case .a8IND; src = base_mem[0xFF00 + base_mem[cpu.PC+1]];
    case; #assert false "Improper LDH Implementation!";
  }

  dest.* = src;
}

gb_instr_nop :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {

}

gb_instr_or :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #assert op1 == .A;

  result := cpu.A;
  #if op2 == {
    case .A; result |= cpu.A;
    case .B; result |= cpu.B;
    case .C; result |= cpu.C;
    case .D; result |= cpu.D;
    case .E; result |= cpu.E;
    case .H; result |= cpu.H;
    case .L; result |= cpu.L;
    case .n8; result |= base_mem[cpu.PC+1];
    case .HLIND; result |= base_mem[cpu.HL];
    case; #assert false "Improper OR implementation!";
  }

  cpu.F = 0;
  cpu.F |= ifx result == 0 then .ZERO;
}

gb_instr_pop :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  low := base_mem[cpu.SP];
  high := base_mem[cpu.SP + 1];
  cpu.SP += 2;

  #if op1 == {
    case .AF; {
      cpu.A = high;
      cpu.F = cast(GB_CPU_Flags) low;
    }
    case .BC; {
      cpu.B = high;
      cpu.C = low;
    }
    case .DE; {
      cpu.D = high;
      cpu.E = low;
    }
    case .HL; {
      cpu.H = high;
      cpu.L = low;
    }

    case; #assert false "Improper POP implementation!";
  }
}

gb_instr_push :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  low := *base_mem[cpu.PC-2];
  high := *base_mem[cpu.PC-1];
  cpu.PC -= 2;
  #if op1 == {
    case .AF; {
      high.* = cpu.A;
      low.* = cast(u8) cpu.F;
    }
    case .BC; {
      high.* = cpu.B;
      low.* = cpu.C;
    }
    case .DE; {
      high.* = cpu.D;
      low.* = cpu.E;
    }
    case .HL; {
      high.* = cpu.H;
      low.* = cpu.L;
    }

    case; #assert false "Improper PUSH implementation!";
  }
}

gb_instr_res :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #assert op1 == .u3 "Improper RES implementation!";
  #if op2 == {
    case .A; cpu.A &= ~cast(u8, 1 << vec_or_u3);
    case .B; cpu.B &= ~cast(u8, 1 << vec_or_u3);
    case .C; cpu.C &= ~cast(u8, 1 << vec_or_u3);
    case .D; cpu.D &= ~cast(u8, 1 << vec_or_u3);
    case .E; cpu.E &= ~cast(u8, 1 << vec_or_u3);
    case .H; cpu.H &= ~cast(u8, 1 << vec_or_u3);
    case .L; cpu.L &= ~cast(u8, 1 << vec_or_u3);
    case .HLIND; base_mem[cpu.HL] &= ~cast(u8, 1 << vec_or_u3);
    case; #assert false "Improper RES implementation!";
  }
}

gb_instr_ret :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  carry_set := cast(bool, cpu.F & .CARRY);
  zero_set := cast(bool, cpu.F & .ZERO);
  should_ret := true;
  #if op1 == {
    case .NONE;

    case .C;  if !carry_set then should_ret = false;
    case .NC; if  carry_set then should_ret = false;
    case .Z;  if !zero_set  then should_ret = false;
    case .NZ; if  zero_set  then should_ret = false;
    case; #assert false "Improper RET implementation";
  }

  if should_ret {
    low := base_mem[cpu.SP];
    high := base_mem[cpu.SP+1];
    cpu.SP += 2;
    cpu.PC = cast(u16, high << 8 | low);
  }
}

// NOTE: This means IME is set *right after* this instruction
gb_instr_reti :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  gb_instr_ei(gb, .NONE, .NONE, 0);
  gb_instr_ret(gb, .NONE, .NONE, 0);
}

gb_instr_rl :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  byte: *u8;
  #if op1 == {
    case .A; byte = *cpu.A;
    case .B; byte = *cpu.B;
    case .C; byte = *cpu.C;
    case .D; byte = *cpu.D;
    case .E; byte = *cpu.E;
    case .H; byte = *cpu.H;
    case .L; byte = *cpu.L;
    case .HLIND; byte = base_mem + cpu.HL;
    case; #assert false "Improper RL implementation!";
  }

  old_carry := cpu.F & .CARRY;
  new_carry := byte.* & 0x80;
  byte.* <<= 1;
  byte.* |= ifx old_carry then cast(u8, 0x01);
  cpu.F = 0;
  cpu.F |= ifx new_carry then .CARRY;
  cpu.F |= ifx byte.* == 0 then .ZERO;
}

gb_instr_rla :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  gb_instr_rl(gb, .A, .NONE, 0);
  cpu.F &= ~.ZERO;
}

gb_instr_rlc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  byte: *u8;
  #if op1 == {
    case .A; byte = *cpu.A;
    case .B; byte = *cpu.B;
    case .C; byte = *cpu.C;
    case .D; byte = *cpu.D;
    case .E; byte = *cpu.E;
    case .H; byte = *cpu.H;
    case .L; byte = *cpu.L;
    case .HLIND; byte = base_mem + cpu.HL;
    case; #assert false "Improper RLC implementation!";
  }

  new_carry := byte.* & 0x80;
  byte.* = (byte.* << 1) | (byte.* >> 7);
  cpu.F = 0;
  cpu.F |= ifx new_carry then .CARRY;
  cpu.F |= ifx byte.* == 0 then .ZERO;
}

gb_instr_rlca :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  gb_instr_rlc(gb, .A, .NONE, 0);
  cpu.F &= ~.ZERO;
}

gb_instr_rr :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  byte: *u8;
  #if op1 == {
    case .A; byte = *cpu.A;
    case .B; byte = *cpu.B;
    case .C; byte = *cpu.C;
    case .D; byte = *cpu.D;
    case .E; byte = *cpu.E;
    case .H; byte = *cpu.H;
    case .L; byte = *cpu.L;
    case .HLIND; byte = base_mem + cpu.HL;
    case; #assert false "Improper RR implementation!";
  }

  old_carry := cpu.F & .CARRY;
  new_carry := byte.* & 0x01;
  byte.* >>= 1;
  byte.* |= ifx old_carry then cast(u8, 0x80);
  cpu.F = 0;
  cpu.F |= ifx new_carry then .CARRY;
  cpu.F |= ifx byte.* == 0 then .ZERO;

}

gb_instr_rra :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  gb_instr_rr(gb, .A, .NONE, 0);
  cpu.F &= ~.ZERO;
}

gb_instr_rrc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  byte: *u8;
  #if op1 == {
    case .A; byte = *cpu.A;
    case .B; byte = *cpu.B;
    case .C; byte = *cpu.C;
    case .D; byte = *cpu.D;
    case .E; byte = *cpu.E;
    case .H; byte = *cpu.H;
    case .L; byte = *cpu.L;
    case .HLIND; byte = base_mem + cpu.HL;
    case; #assert false "Improper RRC implementation!";
  }

  new_carry := byte.* & 0x01;
  byte.* = (byte.* >> 1) | (byte.* << 7);
  cpu.F = 0;
  cpu.F |= ifx new_carry then .CARRY;
  cpu.F |= ifx byte.* == 0 then .ZERO;
}

gb_instr_rrca :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  gb_instr_rrc(gb, .A, .NONE, 0);
  cpu.F &= ~.ZERO;
}

gb_instr_rst :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  next_instr := cpu.PC + 1; // NOTE: Instruction is 1 byte long
  base_mem[cpu.SP - 2] = cast(u8, next_instr & 0xFF);
  base_mem[cpu.SP - 1] = cast(u8, next_instr >> 8);
  cpu.SP -= 2;
  cpu.PC = vec_or_u3;
}

gb_instr_sbc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #assert op1 == .A "Improper SBC implementation!";
  carry := cast(u8) ifx cpu.F & .CARRY then 1 else 0;
  result: u64 = cpu.A;
  r8: u64;

  #if op2 == {
    case .A; r8 = cpu.A + carry;
    case .B; r8 = cpu.B + carry;
    case .C; r8 = cpu.C + carry;
    case .D; r8 = cpu.D + carry;
    case .E; r8 = cpu.E + carry;
    case .H; r8 = cpu.H + carry;
    case .L; r8 = cpu.L + carry;
    case .HLIND; r8 = base_mem[cpu.HL] + carry;
    case .n8; r8 = base_mem[cpu.PC+1] + carry;
    case; #assert false "Improper SBC implementation!";
  }

  result -= r8;
  cpu.F = 0;
  cpu.F |= .SUBTRACTION;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= ifx r8 > cpu.A then .CARRY;
  cpu.F |= ifx r8 & 0xF > cpu.A & 0xF then .HALF_CARRY;

  cpu.A = cast(u8) result;
}

gb_instr_scf :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  cpu.F &= ~.SUBTRACTION;
  cpu.F &= ~.HALF_CARRY;
  cpu.F |= .CARRY;
}

gb_instr_set :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #assert op1 == .u3 "Improper SET implementation!";
  #if op2 == {
    case .A; cpu.A |= (1 << vec_or_u3);
    case .B; cpu.B |= (1 << vec_or_u3);
    case .C; cpu.C |= (1 << vec_or_u3);
    case .D; cpu.D |= (1 << vec_or_u3);
    case .E; cpu.E |= (1 << vec_or_u3);
    case .H; cpu.H |= (1 << vec_or_u3);
    case .L; cpu.L |= (1 << vec_or_u3);
    case .HLIND; base_mem[cpu.HL] |= (1 << vec_or_u3);
    case; #assert false "Improper SET implementation!";
  }
}

gb_instr_sla :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  byte: *u8;
  #if op1 == {
    case .A; byte = *cpu.A;
    case .B; byte = *cpu.B;
    case .C; byte = *cpu.C;
    case .D; byte = *cpu.D;
    case .E; byte = *cpu.E;
    case .H; byte = *cpu.H;
    case .L; byte = *cpu.L;
    case .HLIND; byte = base_mem + cpu.HL;
    case; #assert false "Improper SLA implementation!";
  }

  new_carry := byte.* & 0x80;
  byte.* <<= 1;
  cpu.F = 0;
  cpu.F |= ifx byte.* == 0 then .ZERO;
  cpu.F |= ifx new_carry then .CARRY;
}

gb_instr_sra :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  byte: *s8;
  #if op1 == {
    case .A; byte = cast(*s8) *cpu.A;
    case .B; byte = cast(*s8) *cpu.B;
    case .C; byte = cast(*s8) *cpu.C;
    case .D; byte = cast(*s8) *cpu.D;
    case .E; byte = cast(*s8) *cpu.E;
    case .H; byte = cast(*s8) *cpu.H;
    case .L; byte = cast(*s8) *cpu.L;
    case .HLIND; byte = cast(*s8, base_mem + cpu.HL);
    case; #assert false "Improper SRA implementation!";
  }

  new_carry := byte.* & 0x01;
  byte.* >>= 1;
  cpu.F = 0;
  cpu.F |= ifx byte.* == 0 then .ZERO;
  cpu.F |= ifx new_carry then .CARRY;
}

gb_instr_srl :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  byte: *u8;
  #if op1 == {
    case .A; byte = *cpu.A;
    case .B; byte = *cpu.B;
    case .C; byte = *cpu.C;
    case .D; byte = *cpu.D;
    case .E; byte = *cpu.E;
    case .H; byte = *cpu.H;
    case .L; byte = *cpu.L;
    case .HLIND; byte = base_mem + cpu.HL;
    case; #assert false "Improper SRA implementation!";
  }

  new_carry := byte.* & 0x01;
  byte.* >>= 1;
  cpu.F = 0;
  cpu.F |= ifx byte.* == 0 then .ZERO;
  cpu.F |= ifx new_carry then .CARRY;
}

gb_instr_stop :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  log("GB: Stop instruction received! Shutting down emulator...\n");
}

gb_instr_sub :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #assert op1 == .A "Improper SUB implementation!";
  result: u64 = cpu.A;
  r8: u64;

  #if op2 == {
    case .A; r8 = cpu.A;
    case .B; r8 = cpu.B;
    case .C; r8 = cpu.C;
    case .D; r8 = cpu.D;
    case .E; r8 = cpu.E;
    case .H; r8 = cpu.H;
    case .L; r8 = cpu.L;
    case .HLIND; r8 = base_mem[cpu.HL];
    case .n8; r8 = base_mem[cpu.PC+1];
    case; #assert false "Improper SUB implementation!";
  }

  result -= r8;
  cpu.F = 0;
  cpu.F |= .SUBTRACTION;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= ifx r8 > cpu.A then .CARRY;
  cpu.F |= ifx r8 & 0xF > cpu.A & 0xF then .HALF_CARRY;

  cpu.A = cast(u8) result;
}

gb_instr_swap :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  r8: *u8;
  #if op1 == {
    case .A; r8 = *cpu.A;
    case .B; r8 = *cpu.B;
    case .C; r8 = *cpu.C;
    case .D; r8 = *cpu.D;
    case .E; r8 = *cpu.E;
    case .H; r8 = *cpu.H;
    case .L; r8 = *cpu.L;
    case .HLIND; r8 = base_mem + cpu.HL;
    case; #assert false "Improper SWAP implementation!";
  }

  r8.* = (r8.* << 4) | (r8.* >> 4);
}

gb_instr_xor :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #assert op1 == .A "Improper XOR implementation!";
  r8: u8;
  #if op2 == {
    case .A; r8 = cpu.A;
    case .B; r8 = cpu.B;
    case .C; r8 = cpu.C;
    case .D; r8 = cpu.D;
    case .E; r8 = cpu.E;
    case .H; r8 = cpu.H;
    case .L; r8 = cpu.L;
    case .HLIND; r8 = base_mem[cpu.HL];
    case .n8; r8 = base_mem[cpu.PC+1];
    case; #assert false "Improper XOR implementation!";
  }

  cpu.A = r8 ^ cpu.A;
}

gb_instr_prefix :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {

}


main :: () {
  perm_arena: Flat_Pool;
  context.allocator.proc = flat_pool_allocator_proc;
  context.allocator.data = *perm_arena;
  default_format_int := context.print_style.default_format_int;
  defer context.print_style.default_format_int = default_format_int;
  format_int := *context.print_style.default_format_int;
  format_int.base = 16;

  // TODO: Horrible use of compile time, these should only be generated once and stored somewhere.
  gb_unprefixed_instrs, gb_cbprefixed_instrs :: #run gb_load_instruction_data();
  gb_unprefixed_instr_vtbl, gb_cbprefixed_instr_vtbl := gb_generate_instruction_vtable(gb_unprefixed_instrs, gb_cbprefixed_instrs);
  gb_state: GB_State;
  gb_state.base_mem = cast(*u8) alloc(0xFFFF);

  ROM_START :: 0x0100;
  test_rom, rom_load_result := read_entire_file(TEST_ROM_PATH);
  assert(rom_load_result);
  memcpy(gb_state.base_mem, test_rom.data, test_rom.count);
  gb_state.cpu.PC = ROM_START;

  while true {
    opcode := gb_state.base_mem[gb_state.cpu.PC];
    instr := gb_unprefixed_instrs[opcode];
    if instr.mnemonic == .STOP {
      break;
    }
    gb_instr := gb_unprefixed_instr_vtbl[opcode];
    gb_instr(*gb_state);
    if gb_state.base_mem[0xFF02] == 0x81 {
      out_char := gb_state.base_mem[0xFF01];
      print("%", out_char);
      gb_state.base_mem[0xFF02] = 0x00;
    }
    gb_state.cpu.PC += instr.size;
  }
}