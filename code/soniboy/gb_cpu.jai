GB_CPU_Flags :: enum_flags u8 {
  CARRY       :: 0x10;
  HALF_CARRY  :: 0x20;
  SUBTRACTION :: 0x40;
  ZERO        :: 0x80;
}

GB_Interrupt :: enum_flags u8 {
  VBLANK :: 0x01;
  LCD    :: 0x02;
  TIMER  :: 0x04;
  SERIAL :: 0x08;
  JOYPAD :: 0x10;
}

gb_interrupt_source_lookup :: u8.[0x40, 0x48, 0x50, 0x58, 0x60];

GB_CPU :: struct {
  union {
    AF: u16;
    struct {
      F: GB_CPU_Flags;
      A: u8;
    }
  }
  union {
    BC: u16;
    struct {
      C: u8;
      B: u8;
    }
  }
  union {
    DE: u16;
    struct {
      E: u8;
      D: u8;
    }
  }
  union {
    HL: u16;
    struct {
      L: u8;
      H: u8;
    }
  }
  SP: u16;
  PC: u16;
  IME: bool;
}

GB_State :: struct {
  cpu: GB_CPU;
  base_mem: *u8;
  ime_needs_update: bool;

  gb_unprefixed_instrs, gb_cbprefixed_instrs: [] GB_Instruction;
  gb_unprefixed_instr_vtbl, gb_cbprefixed_instr_vtbl: [] gb_instr_proc;
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

GB_DMG_DIV_INC_RATE :: 16384;
gb_timer_control_lookup :: int.[4096, 262144, 65536, 16384];

gb_pun_u8_s8 :: inline (val: u8) -> s8 {
  punner: union {
    u: u8;
    s: s8;
  };
  punner.u = val;

  return punner.s;
}

gb_operand_is_r8 :: inline (op: GB_Operand) -> bool {
  return op >= .A && op <= .L;
}

gb_operand_is_r16 :: inline (op: GB_Operand) -> bool {
  return op >= .AF && op <= .PC;
}

gb_operand_is_num :: inline (op: GB_Operand) -> bool {
  return op >= .n8 && op <= .e8;
}

gb_operand_is_address_or_indirect :: inline (op: GB_Operand) -> bool {
  return op >= .a8 && op <= .a16IND;
}

gb_instr_proc :: #type (gb: *GB_State);

gb_instr_adc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #assert op1 == .A; "Improper ADC implementation!";
  carry := ifx cast(bool, cpu.F & .CARRY) then 1 else 0;
  result: u64 = cpu.A + cast(u64) carry;
  op: u8;

  #if op2 == {
    case .A; result += cpu.A; op = cpu.A;
    case .B; result += cpu.B; op = cpu.B;
    case .C; result += cpu.C; op = cpu.C;
    case .D; result += cpu.D; op = cpu.D;
    case .E; result += cpu.E; op = cpu.E;
    case .H; result += cpu.H; op = cpu.H;
    case .L; result += cpu.L; op = cpu.L;
    case .HLIND; op = base_mem[cpu.HL]; result += op;
    case .n8; op = base_mem[cpu.PC]; result += op; cpu.PC += 1;
    case; #assert false "Improper ADC implementation!";
  }

  cpu.F = 0;
  cpu.F |= ifx result > 0xFF then .CARRY;
  cpu.F |= ifx (cpu.A & 0xF) + (op & 0xF) + xx carry > 0xF then .HALF_CARRY;
  cpu.A = cast,trunc(u8) result;
  cpu.F |= ifx cpu.A == 0 then .ZERO;
}

gb_instr_add :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #if op1 == .A {
    op: u8;
    result := cast(u64) cpu.A;
    #if op2 == {
      case .A; result += cpu.A; op = cpu.A;
      case .B; result += cpu.B; op = cpu.B;
      case .C; result += cpu.C; op = cpu.C;
      case .D; result += cpu.D; op = cpu.D;
      case .E; result += cpu.E; op = cpu.E;
      case .H; result += cpu.H; op = cpu.H;
      case .L; result += cpu.L; op = cpu.L;
      case .HLIND; op = base_mem[cpu.HL]; result += op;
      case .n8; op = base_mem[cpu.PC]; result += op; cpu.PC += 1;
      case; #assert false "Improper ADD implementation!";
    }
    cpu.F = 0;
    cpu.F |= ifx result > 0xFF then .CARRY;
    cpu.F |= ifx (cpu.A & 0xF) + (op & 0xF) > 0xF then .HALF_CARRY;
    cpu.A = cast,trunc(u8) result;
    cpu.F |= ifx cpu.A == 0 then .ZERO;
  } else #if op1 == .HL {
    op: u16;
    result := cast(u64) cpu.HL;
    #if op2 == {
      case .HL; result += cpu.HL; op = cpu.HL;
      case .BC; result += cpu.BC; op = cpu.BC;
      case .DE; result += cpu.DE; op = cpu.DE;
      case .SP; result += cpu.SP; op = cpu.SP;
      case; #assert false "Improper ADD implementation!";
    }
    cpu.F &= ~.SUBTRACTION;
    cpu.F &= ~.CARRY;
    cpu.F &= ~.HALF_CARRY;
    cpu.F |= ifx result > 0xFFFF then .CARRY;
    cpu.F |= ifx (cpu.HL & 0xFFF) + (op & 0xFFF) > 0xFFF  then .HALF_CARRY;
    cpu.HL = xx,trunc result;
  } else #if op1 == .SP {
    #assert op2 == .e8 "Improper ADD implementation!";
    s_u8 := base_mem[cpu.PC];
    cpu.PC += 1;
    s := gb_pun_u8_s8(s_u8);
    result := cast,trunc(s16) cpu.SP + s;
    cpu.F = 0;
    cpu.F |= ifx (cpu.SP & 0xFF) + s_u8 > 0xFF then .CARRY;
    cpu.F |= ifx (cpu.SP & 0xF) + (s_u8 & 0xF) > 0xF  then .HALF_CARRY;
    cpu.SP = xx,no_check result;
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
    case .n8; result &= base_mem[cpu.PC]; cpu.PC += 1;
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
    next_instr := cpu.PC + 2; // NOTE: Instruction is 3 bytes long
    base_mem[cpu.SP - 2] = cast(u8, next_instr & 0xFF);
    base_mem[cpu.SP - 1] = cast(u8, next_instr >> 8);
    cpu.SP -= 2;
    gb_instr_jp(gb, .a16, .NONE, 0);
  } else {
    cpu.PC += 2;
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
    case .n8; val = base_mem[cpu.PC]; cpu.PC += 1;
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
  cpu.A = xx,trunc result;
  cpu.F |= ifx cpu.A == 0 then .ZERO;
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
    cpu.F |= .SUBTRACTION;
    cpu.F |= ifx result == 0 then .ZERO;
    cpu.F |= ifx result & 0xF > prev_value & 0xF then .HALF_CARRY;
  }
}

gb_instr_di :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  cpu.IME = false;
}

gb_instr_ei :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  ime_needs_update = true;
}

gb_instr_halt :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  assert(false);
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
    cpu.F |= ifx (prev_value & 0xF) + 1 > 0xF then .HALF_CARRY;
  }
}

gb_instr_jp :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  address := #ifx op1 == .a16 || op2 == .a16 {
    cpu.PC += 2;
    (cast(u16, base_mem[cpu.PC-1]) << 8) | base_mem[cpu.PC-2];
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
  offset := gb_pun_u8_s8(base_mem[cpu.PC]);
  cpu.PC += 1;

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
    new_addr := cast(int, cpu.PC) + offset;
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
      address := (cast(u16, base_mem[cpu.PC+1]) << 8) | base_mem[cpu.PC];
      cpu.PC += 2;
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

    case .n8; src = base_mem[cpu.PC]; cpu.PC += 1;
    case .n16; src = (cast(u16, base_mem[cpu.PC+1]) << 8) | base_mem[cpu.PC]; cpu.PC += 2;

    case .HLIND; src = base_mem[cpu.HL];
    case .HLI; src = base_mem[cpu.HL]; cpu.HL += 1;
    case .HLD; src = base_mem[cpu.HL]; cpu.HL -= 1;
    case .BCIND; src = base_mem[cpu.BC];
    case .DEIND; src = base_mem[cpu.DE];
    case .a16IND; {
      address := (cast(u16, base_mem[cpu.PC+1]) << 8) | base_mem[cpu.PC]; cpu.PC += 2;
      src = base_mem[address];
    }

    case .SP_e8; {
      s_u8 := base_mem[cpu.PC];
      cpu.PC += 1;
      s := gb_pun_u8_s8(s_u8);
      rel_addr := cast,trunc(s16) cpu.SP + s;
      src = xx,no_check rel_addr;

      cpu.F = 0;
      cpu.F |= ifx (cpu.SP & 0xFF) + s_u8 > 0xFF then .CARRY;
      cpu.F |= ifx (cpu.SP & 0xF) + (s_u8 & 0xF) > 0xF  then .HALF_CARRY;
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
    case .a8IND; dest = base_mem + 0xFF00 + base_mem[cpu.PC]; cpu.PC += 1;
    case; #assert false "Improper LDH Implementation!";
  }

  #if op2 == {
    case .A; src = cpu.A;
    case .CIND; src = base_mem[0xFF00 + cpu.C];
    case .a8IND; src = base_mem[0xFF00 + base_mem[cpu.PC]]; cpu.PC += 1;
    case; #assert false "Improper LDH Implementation!";
  }

  dest.* = src;
}

gb_instr_nop :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {}

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
    case .n8; result |= base_mem[cpu.PC]; cpu.PC += 1;
    case .HLIND; result |= base_mem[cpu.HL];
    case; #assert false "Improper OR implementation!";
  }

  cpu.F = 0;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.A = result;
}

gb_instr_pop :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  low := base_mem[cpu.SP];
  high := base_mem[cpu.SP + 1];
  cpu.SP += 2;

  #if op1 == {
    case .AF; {
      cpu.A = high;
      cpu.F = cast(GB_CPU_Flags) low;
      cpu.F &= cast,trunc(GB_CPU_Flags) ~0xF;
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
  low := *base_mem[cpu.SP-2];
  high := *base_mem[cpu.SP-1];
  cpu.SP -= 2;
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

    // NOTE: For internal emulator use
    case .PC; {
      high.* = xx (cpu.PC >> 8);
      low.* = xx (cpu.PC & 0xFF);
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
    cpu.PC = (cast(u16, high) << 8) | low;
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
  next_instr := cpu.PC;
  base_mem[cpu.SP - 2] = cast(u8, next_instr & 0xFF);
  base_mem[cpu.SP - 1] = cast(u8, next_instr >> 8);
  cpu.SP -= 2;
  cpu.PC = vec_or_u3;
}

gb_instr_sbc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {
  #assert op1 == .A "Improper SBC implementation!";
  carry := cast(u8) ifx cpu.F & .CARRY then 1 else 0;
  result := cpu.A;
  r8: u16;

  #if op2 == {
    case .A; r8 = cpu.A;
    case .B; r8 = cpu.B;
    case .C; r8 = cpu.C;
    case .D; r8 = cpu.D;
    case .E; r8 = cpu.E;
    case .H; r8 = cpu.H;
    case .L; r8 = cpu.L;
    case .HLIND; r8 = base_mem[cpu.HL];
    case .n8; r8 = base_mem[cpu.PC]; cpu.PC += 1;
    case; #assert false "Improper SBC implementation!";
  }

  result -= xx r8 + carry;
  cpu.F = 0;
  cpu.F |= .SUBTRACTION;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= ifx r8 + carry > cpu.A then .CARRY;
  cpu.F |= ifx (r8 & 0xF) + carry > (cpu.A & 0xF) then .HALF_CARRY;

  cpu.A = result;
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
    case .n8; r8 = base_mem[cpu.PC]; cpu.PC += 1;
    case; #assert false "Improper SUB implementation!";
  }

  result -= r8;
  cpu.F = 0;
  cpu.F |= .SUBTRACTION;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= ifx r8 > cpu.A then .CARRY;
  cpu.F |= ifx r8 & 0xF > cpu.A & 0xF then .HALF_CARRY;

  cpu.A = cast,trunc(u8) result;
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
  cpu.F = 0;
  cpu.F |= ifx r8.* == 0 then .ZERO;
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
    case .n8; r8 = base_mem[cpu.PC]; cpu.PC += 1;
    case; #assert false "Improper XOR implementation!";
  }

  cpu.A = r8 ^ cpu.A;
  cpu.F = 0;
  cpu.F |= ifx cpu.A == 0 then .ZERO;
}

gb_instr_prefix :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) {}

gb_request_interrupt :: (using gb: *GB_State, interrupt: GB_Interrupt) {
  base_mem[0xFF0F] |= xx interrupt;
}

gb_log_state :: (using gb: *GB_State) {
  log("A:% F:% B:% C:% D:% E:% H:% L:% SP:% PC:% PCMEM:%,%,%,%",
    cpu.A, cast(u8) cpu.F, cpu.B, cpu.C, cpu.D, cpu.E, cpu.H, cpu.L,
    FormatInt.{value=cpu.SP, minimum_digits=4, base=16}, FormatInt.{value=cpu.PC, minimum_digits=4, base=16},
    base_mem[cpu.PC], base_mem[cpu.PC+1], base_mem[cpu.PC+2], base_mem[cpu.PC+3]);
}

gb_init :: () -> GB_State {
  gb_state: GB_State;

  // TODO: These should only be generated once and stored somewhere.
  gb_unprefixed_instrs, gb_cbprefixed_instrs := gb_load_instruction_data();
  gb_unprefixed_instr_vtbl, gb_cbprefixed_instr_vtbl := gb_generate_instruction_vtable();
  gb_state.gb_unprefixed_instrs = gb_unprefixed_instrs;
  gb_state.gb_cbprefixed_instrs = gb_cbprefixed_instrs;
  gb_state.gb_unprefixed_instr_vtbl = gb_unprefixed_instr_vtbl;
  gb_state.gb_cbprefixed_instr_vtbl = gb_cbprefixed_instr_vtbl;

  gb_state.base_mem = cast(*u8) alloc(0x10000);
  assert (gb_state.base_mem != null);

  test_rom, rom_load_result := read_entire_file(TEST_ROM_PATH,, temp);
  assert(rom_load_result);
  memcpy(gb_state.base_mem, test_rom.data, test_rom.count);

  gb_state.base_mem[0xFF44] = 0x90;
  gb_state.base_mem[0xFF07] = 0xF8;
  gb_state.cpu.A = 0x01;
  gb_state.cpu.F = 0xB0;
  gb_state.cpu.B = 0x00;
  gb_state.cpu.C = 0x13;
  gb_state.cpu.D = 0x00;
  gb_state.cpu.E = 0xD8;
  gb_state.cpu.H = 0x01;
  gb_state.cpu.L = 0x4D;
  gb_state.cpu.SP = 0xFFFE;
  gb_state.cpu.PC = 0x0100;

  #if GB_DOCTOR_LOG gb_log_state(*gb_state);

  return gb_state;
}

gb_step :: (using gb_state: *GB_State) {
  // NOTE: Fetch & execute next instruction
  opcode := base_mem[cpu.PC];
  cpu.PC += 1;

  instr := gb_unprefixed_instrs[opcode];
  gb_instr := gb_unprefixed_instr_vtbl[opcode];

  if instr.mnemonic == .PREFIX {
    opcode = base_mem[cpu.PC];
    cpu.PC += 1;
    instr = gb_cbprefixed_instrs[opcode];
    gb_instr = gb_cbprefixed_instr_vtbl[opcode];
  }
  gb_instr(gb_state);
  #if GB_DOCTOR_LOG gb_log_state(gb_state);

  // NOTE: Check for interrupts
  if instr.mnemonic != .EI && ime_needs_update {
    ime_needs_update = false;
    cpu.IME = true;
  }

  if cpu.IME {
    pending_interrupts := base_mem[0xFF0F];
    enabled_interrupts := base_mem[0xFFFF];
    interrupts := type_info(GB_Interrupt);
    for < interrupts.values {
      if (pending_interrupts & enabled_interrupts) & cast(u8) it {
        base_mem[0xFF0F] &= ~cast(u8) it;
        cpu.IME = false;
        gb_instr_push(gb_state, .PC, .NONE, 0);
        cpu.PC = gb_interrupt_source_lookup[it_index];
      }
    }
  }

  // NOTE: Test ROM logging
  if base_mem[0xFF02] == 0x81 {
    out_char := base_mem[0xFF01];
    #if !GB_DOCTOR_LOG then print("%", to_string(*out_char,1));
    base_mem[0xFF02] = 0x00;
  }
}