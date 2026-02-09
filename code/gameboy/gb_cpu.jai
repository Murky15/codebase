/*  Tests Passed
  - [X] cpu_instrs
  - [ ] dmg_sound
*/

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

GB_Memory_Bank_Controller :: enum {
  ROM_ONLY;
  MBC1;
  MBC2;
}

GB_Cartridge_Bank_Info :: struct {
  mbc_type: GB_Memory_Bank_Controller;
  rom_size: int;
  ram_size: int;
}

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
  low_power_mode: bool;
  halt_stunt_bug: bool;

  div\ _accum: int;
  timer_accum: int;

  cartridge: [] u8;
  bank_info: GB_Cartridge_Bank_Info;
  active_bank: u8;

  unprefixed_instrs,     cbprefixed_instrs: [] GB_Instruction;
  unprefixed_instr_vtbl, cbprefixed_instr_vtbl: [] gb_instr_proc;
}

GB_Instruction :: struct {
  mnemonic: GB_Mnemonic;
  size: u16;
  t_states: int;
  op1: GB_Operand;
  op2: GB_Operand;
  vec_or_u3: u8;

  #overlay (op1) op: [2] GB_Operand;
}

GB_ROM_BANK_SIZE :: 16 * 1024;
GB_DMG_MASTER_CLOCK_RATE :: 4194304;
GB_DMG_DIV\  _CLOCK_RATE :: 16384;
GB_CLOCK_CYCLES_PER_DIV\ _INC :: GB_DMG_MASTER_CLOCK_RATE / GB_DMG_DIV_CLOCK_RATE;
GB_CLOCK_CYCLES_PER_TIMER_INC :: int.[GB_DMG_MASTER_CLOCK_RATE/4096,
                                      GB_DMG_MASTER_CLOCK_RATE/262144,
                                      GB_DMG_MASTER_CLOCK_RATE/65536,
                                      GB_DMG_MASTER_CLOCK_RATE/16384];
GB_INTERRUPT_SOURCE_LOOKUP :: u8.[0x40, 0x48, 0x50, 0x58, 0x60];

gb_pun_byte :: inline ($Src: Type, $Ret: Type, val: Src) -> Ret {
  punner: union {
    u: u8;
    s: s8;
  };
  #if Src == u8 {
    punner.u = val;
  } else #if Src == s8 {
    punner.s = val;
  } else {
    #assert false "Only 1-byte values are applicable here!";
  }

  return #ifx Ret == s8 punner.s else punner.u;
}
gb_pun_u8_s8 :: #bake_arguments gb_pun_byte(Src=u8, Ret=s8);
gb_pun_s8_u8 :: #bake_arguments gb_pun_byte(Src=s8, Ret=u8);

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

gb_instr_proc :: #type (gb: *GB_State) -> conditional_t_states: int;

gb_instr_adc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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
    case .HLIND; op = gb_read(gb, cpu.HL); result += op;
    case .n8; op = gb_read_pc(gb); result += op;
    case; #assert false "Improper ADC implementation!";
  }

  cpu.F = 0;
  cpu.F |= ifx result > 0xFF then .CARRY;
  cpu.F |= ifx (cpu.A & 0xF) + (op & 0xF) + xx carry > 0xF then .HALF_CARRY;
  cpu.A = cast,trunc(u8) result;
  cpu.F |= ifx cpu.A == 0 then .ZERO;

  return 0;
}

gb_instr_add :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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
      case .HLIND; op = gb_read(gb, cpu.HL); result += op;
      case .n8; op = gb_read_pc(gb); result += op;
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
    s_u8 := gb_read_pc(gb);
    s := gb_pun_u8_s8(s_u8);
    result := cast,trunc(s16) cpu.SP + s;
    cpu.F = 0;
    cpu.F |= ifx (cpu.SP & 0xFF) + s_u8 > 0xFF then .CARRY;
    cpu.F |= ifx (cpu.SP & 0xF) + (s_u8 & 0xF) > 0xF  then .HALF_CARRY;
    cpu.SP = xx,no_check result;
  } else #assert false "Improper ADD implementation!";

  return 0;
}

gb_instr_and :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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
    case .HLIND; result &= gb_read(gb, cpu.HL);
    case .n8; result &= gb_read_pc(gb);
    case; #assert false "Improper AND implementation!";
  }

  cpu.F = 0;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= .HALF_CARRY;
  cpu.A = result;

  return 0;
}

gb_instr_bit :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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
    case .HLIND; bit_set = cast(bool, (gb_read(gb, cpu.HL) >> vec_or_u3) & 0x1);
    case; #assert false "Improper BIT implementation!";
  }

  cpu.F &= ~.ZERO;
  cpu.F &= ~.SUBTRACTION;
  cpu.F |= ifx !bit_set then .ZERO;
  cpu.F |= .HALF_CARRY;

  return 0;
}

gb_instr_call :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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
  return ifx should_call {
    next_instr := cpu.PC + 2; // NOTE: Instruction is 3 bytes long
    gb_write(gb, cpu.SP - 2, cast(u8, next_instr & 0xFF));
    gb_write(gb, cpu.SP - 1, cast(u8, next_instr >> 8));
    cpu.SP -= 2;
    gb_instr_jp(gb, .a16, .NONE, 0);
    24;
  } else {
    cpu.PC += 2;
    12;
  };
}

gb_instr_ccf :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  cpu.F &= ~.SUBTRACTION;
  cpu.F &= ~.HALF_CARRY;
  cpu.F ^= .CARRY;

  return 0;
}

gb_instr_cp :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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
    case .HLIND; val = gb_read(gb, cpu.HL);
    case .n8; val = gb_read_pc(gb);;
    case; #assert false "Improper CP implementation!";
  }
  result -= val;

  cpu.F = 0;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= .SUBTRACTION;
  cpu.F |= ifx val > cpu.A then .CARRY;
  cpu.F |= ifx val & 0xF > cpu.A & 0xF then .HALF_CARRY;

  return 0;
}

gb_instr_cpl :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  cpu.A = ~cpu.A;
  cpu.F |= .SUBTRACTION;
  cpu.F |= .HALF_CARRY;

  return 0;
}

gb_instr_daa :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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

  return 0;
}

gb_instr_dec :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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
      result = gb_read(gb, cpu.HL) - 1;
      prev_value = gb_read(gb, cpu.HL);
      gb_write(gb, cpu.HL, xx result);
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

  return 0;
}

gb_instr_di :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  cpu.IME = false;

  return 0;
}

gb_instr_ei :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  ime_needs_update = true;

  return 0;
}

gb_instr_halt :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  if cpu.IME {
    low_power_mode = true;
  } else {
    interrupts_pending := gb_read(gb, 0xFF0F) & gb_read(gb, 0xFFFF);
    if interrupts_pending then halt_stunt_bug = true; else low_power_mode = true;
  }

  return 0;
}

gb_instr_inc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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
      result = gb_read(gb, cpu.HL) + 1;
      prev_value = gb_read(gb, cpu.HL);
      gb_write(gb, cpu.HL, xx result);
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

  return 0;
}

gb_instr_jp :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  address := #ifx op1 == .a16 || op2 == .a16 {
    low := gb_read_pc(gb);
    high := gb_read_pc(gb);
    (cast(u16, high) << 8) | low;
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

  return ifx should_jump {
    cpu.PC = address;
    ifx op1 == .HL 4 else 16;
  } else 12;
}

gb_instr_jr :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  offset := gb_pun_u8_s8(gb_read_pc(gb));

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

  return ifx should_jump {
    new_addr := cast(int, cpu.PC) + offset;
    cpu.PC = xx new_addr;
    12;
  } else 8;
}

gb_instr_ld :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  src_type :: #ifx #run (gb_operand_is_r8(op1) || gb_operand_is_address_or_indirect(op1)) then u8 else u16;
  dest_type :: #ifx #run gb_operand_is_address_or_indirect(op1) u16 else *src_type;
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

    case .BCIND; dest = cpu.BC;
    case .DEIND; dest = cpu.DE;
    case .HLIND; dest = cpu.HL;
    case .HLI; dest = cpu.HL; cpu.HL += 1;
    case .HLD; dest = cpu.HL; cpu.HL -= 1;
    case .a16IND; {
      low := gb_read_pc(gb);
      high := gb_read_pc(gb);
      address := (cast(u16, high) << 8) | low;
      dest = address;
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
      gb_write(gb, dest, cast(u8, cpu.SP & 0xFF));
      gb_write(gb, dest + 1, cast(u8, cpu.SP >> 8));
      return 0;
    }

    case .n8; src = gb_read_pc(gb);
    case .n16; {
      low := gb_read_pc(gb);
      high := gb_read_pc(gb);
      src = (cast(u16, high) << 8) | low;
    }

    case .HLIND; src = gb_read(gb, cpu.HL);
    case .HLI; src = gb_read(gb, cpu.HL); cpu.HL += 1;
    case .HLD; src = gb_read(gb, cpu.HL); cpu.HL -= 1;
    case .BCIND; src = gb_read(gb, cpu.BC);
    case .DEIND; src = gb_read(gb, cpu.DE);
    case .a16IND; {
      low := gb_read_pc(gb);
      high := gb_read_pc(gb);
      address := (cast(u16, high) << 8) | low;
      src = gb_read(gb, address);
    }
    case .SP_e8; {
      s_u8 := gb_read_pc(gb);
      s := gb_pun_u8_s8(s_u8);
      rel_addr := cast,trunc(s16) cpu.SP + s;
      src = xx,no_check rel_addr;

      cpu.F = 0;
      cpu.F |= ifx (cpu.SP & 0xFF) + s_u8 > 0xFF then .CARRY;
      cpu.F |= ifx (cpu.SP & 0xF) + (s_u8 & 0xF) > 0xF  then .HALF_CARRY;
    }

    case; #assert false "Improper LD Implementation!";
  }
  #if #run gb_operand_is_address_or_indirect(op1) {
    gb_write(gb, dest, src);
  } else {
    dest.* = src;
  }

  return 0;
}

gb_instr_ldh :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  dest_type :: #ifx op1 == .A then *u8 else u16;
  dest: dest_type;
  src: u8;
  #if op1 == {
    case .A; dest = *cpu.A;
    case .CIND; dest = 0xFF00 + cast(u16) cpu.C;
    case .a8IND; dest = 0xFF00 + cast(u16) gb_read_pc(gb);
    case; #assert false "Improper LDH Implementation!";
  }

  #if op2 == {
    case .A; src = cpu.A;
    case .CIND; src = gb_read(gb, 0xFF00 + cast(u16) cpu.C);
    case .a8IND; src = gb_read(gb, 0xFF00 + cast(u16) gb_read_pc(gb));
    case; #assert false "Improper LDH Implementation!";
  }
  #if op1 == .A {
    dest.* = src;
  } else {
    gb_write(gb, dest, src);
  }

  return 0;
}

gb_instr_nop :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int { return 0; }

gb_instr_or :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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
    case .n8; result |= gb_read_pc(gb);
    case .HLIND; result |= gb_read(gb, cpu.HL);
    case; #assert false "Improper OR implementation!";
  }

  cpu.F = 0;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.A = result;

  return 0;
}

gb_instr_pop :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  low := gb_read(gb, cpu.SP);
  high := gb_read(gb, cpu.SP + 1);
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

  return 0;
}

gb_instr_push :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  low := cpu.SP-2;
  high := cpu.SP-1;
  cpu.SP -= 2;
  #if op1 == {
    case .AF; {
      gb_write(gb, high, cpu.A);
      gb_write(gb, low, cast(u8) cpu.F);
    }
    case .BC; {
      gb_write(gb, high, cpu.B);
      gb_write(gb, low, cpu.C);
    }
    case .DE; {
      gb_write(gb, high, cpu.D);
      gb_write(gb, low, cpu.E);
    }
    case .HL; {
      gb_write(gb, high, cpu.H);
      gb_write(gb, low, cpu.L);
    }

    // NOTE: For internal emulator use
    case .PC; {
      gb_write(gb, high, xx (cpu.PC >> 8));
      gb_write(gb, low, xx (cpu.PC & 0xFF));
    }

    case; #assert false "Improper PUSH implementation!";
  }

  return 0;
}

gb_instr_res :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  #assert op1 == .u3 "Improper RES implementation!";
  #if op2 == {
    case .A; cpu.A &= ~cast(u8, 1 << vec_or_u3);
    case .B; cpu.B &= ~cast(u8, 1 << vec_or_u3);
    case .C; cpu.C &= ~cast(u8, 1 << vec_or_u3);
    case .D; cpu.D &= ~cast(u8, 1 << vec_or_u3);
    case .E; cpu.E &= ~cast(u8, 1 << vec_or_u3);
    case .H; cpu.H &= ~cast(u8, 1 << vec_or_u3);
    case .L; cpu.L &= ~cast(u8, 1 << vec_or_u3);
    case .HLIND; {
      val := gb_read(gb, cpu.HL);
      val &= ~cast(u8, 1 << vec_or_u3);
      gb_write(gb, cpu.HL, val);
    }
    case; #assert false "Improper RES implementation!";
  }

  return 0;
}

gb_instr_ret :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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

  t_states := ifx op1 != .NONE 16;
  if should_ret {
    low := gb_read(gb, cpu.SP);
    high := gb_read(gb, cpu.SP + 1);
    cpu.SP += 2;
    cpu.PC = (cast(u16, high) << 8) | low;
  }

  return ifx t_states t_states else ifx should_ret 20 else 8;
}

// NOTE: This means IME is set *right after* this instruction
gb_instr_reti :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  gb_instr_ei(gb, .NONE, .NONE, 0);
  gb_instr_ret(gb, .NONE, .NONE, 0);

  return 0;
}

gb_instr_rl :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  type :: #ifx #run gb_operand_is_r8(op1) then *u8 else u16;
  byte: type;
  #if op1 == {
    case .A; byte = *cpu.A;
    case .B; byte = *cpu.B;
    case .C; byte = *cpu.C;
    case .D; byte = *cpu.D;
    case .E; byte = *cpu.E;
    case .H; byte = *cpu.H;
    case .L; byte = *cpu.L;
    case .HLIND; byte = cpu.HL;
    case; #assert false "Improper RL implementation!";
  }

  old_carry := cpu.F & .CARRY;
  cpu.F = 0;
  #if #run gb_operand_is_r8(op1) {
    new_carry := byte.* & 0x80;
    byte.* <<= 1;
    byte.* |= ifx old_carry then cast(u8, 0x01);
    cpu.F |= ifx byte.* == 0 then .ZERO;
  } else {
    dest := gb_read(gb, byte);
    new_carry := dest & 0x80;
    dest <<= 1;
    dest |= ifx old_carry then cast(u8, 0x01);
    gb_write(gb, byte, dest);
    cpu.F |= ifx dest == 0 then .ZERO;
  }
  cpu.F |= ifx new_carry then .CARRY;

  return 0;
}

gb_instr_rla :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  gb_instr_rl(gb, .A, .NONE, 0);
  cpu.F &= ~.ZERO;

  return 0;
}

gb_instr_rlc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  type :: #ifx #run gb_operand_is_r8(op1) then *u8 else u16;
  byte: type;
  #if op1 == {
    case .A; byte = *cpu.A;
    case .B; byte = *cpu.B;
    case .C; byte = *cpu.C;
    case .D; byte = *cpu.D;
    case .E; byte = *cpu.E;
    case .H; byte = *cpu.H;
    case .L; byte = *cpu.L;
    case .HLIND; byte = cpu.HL;
    case; #assert false "Improper RLC implementation!";
  }

  cpu.F = 0;
  #ifx #run gb_operand_is_r8(op1) {
    new_carry := byte.* & 0x80;
    byte.* = (byte.* << 1) | (byte.* >> 7);
    cpu.F |= ifx byte.* == 0 then .ZERO;
  } else {
    dest := gb_read(gb, byte);
    new_carry := dest & 0x80;
    dest = (dest << 1) | (dest >> 7);
    gb_write(gb, byte, dest);
    cpu.F |= ifx dest == 0 then .ZERO;
  }
  cpu.F |= ifx new_carry then .CARRY;

  return 0;
}

gb_instr_rlca :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  gb_instr_rlc(gb, .A, .NONE, 0);
  cpu.F &= ~.ZERO;

  return 0;
}

gb_instr_rr :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  type :: #ifx #run gb_operand_is_r8(op1) then *u8 else u16;
  byte: type;
  #if op1 == {
    case .A; byte = *cpu.A;
    case .B; byte = *cpu.B;
    case .C; byte = *cpu.C;
    case .D; byte = *cpu.D;
    case .E; byte = *cpu.E;
    case .H; byte = *cpu.H;
    case .L; byte = *cpu.L;
    case .HLIND; byte = cpu.HL;
    case; #assert false "Improper RR implementation!";
  }

  old_carry := cpu.F & .CARRY;
  cpu.F = 0;
  #ifx #run gb_operand_is_r8(op1) {
    new_carry := byte.* & 0x01;
    byte.* >>= 1;
    byte.* |= ifx old_carry then cast(u8, 0x80);
    cpu.F |= ifx byte.* == 0 then .ZERO;
  } else {
    dest := gb_read(gb, byte);
    new_carry := dest & 0x01;
    dest >>= 1;
    dest |= ifx old_carry then cast(u8, 0x80);
    gb_write(gb, byte, dest);
    cpu.F |= ifx dest == 0 then .ZERO;
  }
  cpu.F |= ifx new_carry then .CARRY;

  return 0;
}

gb_instr_rra :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  gb_instr_rr(gb, .A, .NONE, 0);
  cpu.F &= ~.ZERO;

  return 0;
}

gb_instr_rrc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  type :: #ifx #run gb_operand_is_r8(op1) then *u8 else u16;
  byte: type;
  #if op1 == {
    case .A; byte = *cpu.A;
    case .B; byte = *cpu.B;
    case .C; byte = *cpu.C;
    case .D; byte = *cpu.D;
    case .E; byte = *cpu.E;
    case .H; byte = *cpu.H;
    case .L; byte = *cpu.L;
    case .HLIND; byte = cpu.HL;
    case; #assert false "Improper RRC implementation!";
  }

  cpu.F = 0;
  #ifx #run gb_operand_is_r8(op1) {
    new_carry := byte.* & 0x01;
    byte.* = (byte.* >> 1) | (byte.* << 7);
    cpu.F |= ifx byte.* == 0 then .ZERO;
  } else {
    dest := gb_read(gb, byte);
    new_carry := dest & 0x01;
    dest = (dest >> 1) | (dest << 7);
    gb_write(gb, byte, dest);
    cpu.F |= ifx dest == 0 then .ZERO;
  }
  cpu.F |= ifx new_carry then .CARRY;

  return 0;
}

gb_instr_rrca :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  gb_instr_rrc(gb, .A, .NONE, 0);
  cpu.F &= ~.ZERO;

  return 0;
}

gb_instr_rst :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  next_instr := cpu.PC;
  gb_write(gb, cpu.SP - 2, cast(u8, next_instr & 0xFF));
  gb_write(gb, cpu.SP - 1, cast(u8, next_instr >> 8));
  cpu.SP -= 2;
  cpu.PC = vec_or_u3;

  return 0;
}

gb_instr_sbc :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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
    case .HLIND; r8 = gb_read(gb, cpu.HL);
    case .n8; r8 = gb_read_pc(gb);
    case; #assert false "Improper SBC implementation!";
  }

  result -= xx r8 + carry;
  cpu.F = 0;
  cpu.F |= .SUBTRACTION;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= ifx r8 + carry > cpu.A then .CARRY;
  cpu.F |= ifx (r8 & 0xF) + carry > (cpu.A & 0xF) then .HALF_CARRY;

  cpu.A = result;

  return 0;
}

gb_instr_scf :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  cpu.F &= ~.SUBTRACTION;
  cpu.F &= ~.HALF_CARRY;
  cpu.F |= .CARRY;

  return 0;
}

gb_instr_set :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  #assert op1 == .u3 "Improper SET implementation!";
  #if op2 == {
    case .A; cpu.A |= (1 << vec_or_u3);
    case .B; cpu.B |= (1 << vec_or_u3);
    case .C; cpu.C |= (1 << vec_or_u3);
    case .D; cpu.D |= (1 << vec_or_u3);
    case .E; cpu.E |= (1 << vec_or_u3);
    case .H; cpu.H |= (1 << vec_or_u3);
    case .L; cpu.L |= (1 << vec_or_u3);
    case .HLIND; {
      val := gb_read(gb, cpu.HL);
      val |= (1 << vec_or_u3);
      gb_write(gb, cpu.HL, val);
    }
    case; #assert false "Improper SET implementation!";
  }

  return 0;
}

gb_instr_sla :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  type :: #ifx #run gb_operand_is_r8(op1) then *u8 else u16;
  byte: type;
  #if op1 == {
    case .A; byte = *cpu.A;
    case .B; byte = *cpu.B;
    case .C; byte = *cpu.C;
    case .D; byte = *cpu.D;
    case .E; byte = *cpu.E;
    case .H; byte = *cpu.H;
    case .L; byte = *cpu.L;
    case .HLIND; byte = cpu.HL;
    case; #assert false "Improper SLA implementation!";
  }

  cpu.F = 0;
  #ifx #run gb_operand_is_r8(op1) {
    new_carry := byte.* & 0x80;
    byte.* <<= 1;
    cpu.F |= ifx byte.* == 0 then .ZERO;
  } else {
    dest := gb_read(gb, byte);
    new_carry := dest & 0x80;
    dest <<= 1;
    gb_write(gb, byte, dest);
    cpu.F |= ifx dest == 0 then .ZERO;
  }
  cpu.F |= ifx new_carry then .CARRY;

  return 0;
}

gb_instr_sra :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  type :: #ifx #run gb_operand_is_r8(op1) then *s8 else u16;
  byte: type;
  #if op1 == {
    case .A; byte = cast(*s8) *cpu.A;
    case .B; byte = cast(*s8) *cpu.B;
    case .C; byte = cast(*s8) *cpu.C;
    case .D; byte = cast(*s8) *cpu.D;
    case .E; byte = cast(*s8) *cpu.E;
    case .H; byte = cast(*s8) *cpu.H;
    case .L; byte = cast(*s8) *cpu.L;
    case .HLIND; byte = cpu.HL;
    case; #assert false "Improper SRA implementation!";
  }

  cpu.F = 0;
  #ifx #run gb_operand_is_r8(op1) {
    new_carry := byte.* & 0x01;
    byte.* >>= 1;
    cpu.F |= ifx byte.* == 0 then .ZERO;
  } else {
    dest := gb_pun_u8_s8(gb_read(gb, byte));
    new_carry := dest & 0x01;
    dest >>= 1;
    gb_write(gb, byte, gb_pun_s8_u8(dest));
    cpu.F |= ifx dest == 0 then .ZERO;
  }
  cpu.F |= ifx new_carry then .CARRY;

  return 0;
}

gb_instr_srl :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  type :: #ifx #run gb_operand_is_r8(op1) then *u8 else u16;
  byte: type;
  #if op1 == {
    case .A; byte = *cpu.A;
    case .B; byte = *cpu.B;
    case .C; byte = *cpu.C;
    case .D; byte = *cpu.D;
    case .E; byte = *cpu.E;
    case .H; byte = *cpu.H;
    case .L; byte = *cpu.L;
    case .HLIND; byte = cpu.HL;
    case; #assert false "Improper SRA implementation!";
  }

  cpu.F = 0;
  #ifx #run gb_operand_is_r8(op1) {
    new_carry := byte.* & 0x01;
    byte.* >>= 1;
    cpu.F |= ifx byte.* == 0 then .ZERO;
  } else {
    dest := gb_read(gb, byte);
    new_carry := dest & 0x01;
    dest >>= 1;
    gb_write(gb, byte, dest);
    cpu.F |= ifx dest == 0 then .ZERO;
  }
  cpu.F |= ifx new_carry then .CARRY;

  return 0;
}

gb_instr_stop :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  return 0;
}

gb_instr_sub :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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
    case .HLIND; r8 = gb_read(gb, cpu.HL);
    case .n8; r8 = gb_read_pc(gb);
    case; #assert false "Improper SUB implementation!";
  }

  result -= r8;
  cpu.F = 0;
  cpu.F |= .SUBTRACTION;
  cpu.F |= ifx result == 0 then .ZERO;
  cpu.F |= ifx r8 > cpu.A then .CARRY;
  cpu.F |= ifx r8 & 0xF > cpu.A & 0xF then .HALF_CARRY;

  cpu.A = cast,trunc(u8) result;

  return 0;
}

gb_instr_swap :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
  type :: #ifx #run gb_operand_is_r8(op1) then *u8 else u16;
  r8: type;
  #if op1 == {
    case .A; r8 = *cpu.A;
    case .B; r8 = *cpu.B;
    case .C; r8 = *cpu.C;
    case .D; r8 = *cpu.D;
    case .E; r8 = *cpu.E;
    case .H; r8 = *cpu.H;
    case .L; r8 = *cpu.L;
    case .HLIND; r8 = cpu.HL;
    case; #assert false "Improper SWAP implementation!";
  }

  cpu.F = 0;
  #ifx #run gb_operand_is_r8(op1) {
    r8.* = (r8.* << 4) | (r8.* >> 4);
    cpu.F |= ifx r8.* == 0 then .ZERO;
  } else {
    dest := gb_read(gb, r8);
    dest = (dest << 4) | (dest >> 4);
    gb_write(gb, r8, dest);
    cpu.F |= ifx dest == 0 then .ZERO;
  }

  return 0;
}

gb_instr_xor :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int {
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
    case .HLIND; r8 = gb_read(gb, cpu.HL);
    case .n8; r8 = gb_read_pc(gb);
    case; #assert false "Improper XOR implementation!";
  }

  cpu.A = r8 ^ cpu.A;
  cpu.F = 0;
  cpu.F |= ifx cpu.A == 0 then .ZERO;

  return 0;
}

gb_instr_prefix :: (using gb: *GB_State, $op1: GB_Operand, $op2: GB_Operand, $vec_or_u3: u8) -> int { return 0; }

gb_request_interrupt :: (gb: *GB_State, interrupt: GB_Interrupt) {
  gb.base_mem[0xFF0F] |= xx interrupt;
}

gb_read :: (gb: *GB_State, addr: u16) -> u8 {
  return gb.base_mem[addr];
}

gb_read_pc :: (gb: *GB_State) -> u8 {
  defer gb.cpu.PC += 1;
  return gb_read(gb, gb.cpu.PC);
}

gb_write :: (gb: *GB_State, addr: u16, value: u8) {
  if gb.bank_info.mbc_type == .MBC1 {
    return_early := true;
    rom_bank_ready := false;
    if addr < 0x2000 { // NOTE: RAM Enable
      log("Wrote: % to RAM Enable", value);
      assert(false, "GB Error: This bank mode is not yet implemented!");
    } else if addr >= 0x2000 && addr < 0x4000 { // NOTE: ROM Bank Number
      if (value & 0x1F) == 0 then value = 1; // Yes this is intentional
      num_banks_supported := cast(u64, gb.bank_info.rom_size / GB_ROM_BANK_SIZE);
      bits_required := cast(u8) bit_scan_forward(num_banks_supported) - 1;
      value &= cast(u8) min(num_banks_supported - 1, 0x1F);
      gb.active_bank = value;
      rom_bank_ready = bits_required <= 5; // Otherwise we need to wait and see what the upper bits are
    } else if addr >= 0x4000 && addr < 0x6000 { // NOTE: RAM Bank Number / Upper ROM Bank Number
      log("Wrote: % to RAM Bank Number / Upper ROM Bank Number", value);
      assert(false, "GB Error: This bank mode is not yet implemented!");
    } else if addr >= 0x6000 && addr < 0x8000 { // NOTE: Banking Mode Select
      log("Wrote: % to Banking Mode Select", value);
      assert(false, "GB Error: This bank mode is not yet implemented!");
    } else {
      return_early = false;
    }

    if rom_bank_ready {
      if context.log_level >= .VERBOSE log("Accessing ROM bank: %", gb.active_bank);
      rom_bank_location := GB_ROM_BANK_SIZE * gb.active_bank;
      memcpy(gb.base_mem + 0x4000, *gb.cartridge[rom_bank_location], GB_ROM_BANK_SIZE);
    }

    if return_early then return;

  } else if gb.bank_info.mbc_type != .ROM_ONLY then assert(false, "GB Error: This MBC mode (%) is not yet implemented!", gb.bank_info.mbc_type);

  if addr == 0xFF04 then value = 0;
  if addr == 0xFF07 then gb.timer_accum = 0;
  gb.base_mem[addr] = value;
}

gb_log_state :: (using gb: *GB_State) {
  log("A:% F:% B:% C:% D:% E:% H:% L:% SP:% PC:% PCMEM:%,%,%,%",
    cpu.A, cast(u8) cpu.F, cpu.B, cpu.C, cpu.D, cpu.E, cpu.H, cpu.L,
    FormatInt.{value=cpu.SP, minimum_digits=4, base=16}, FormatInt.{value=cpu.PC, minimum_digits=4, base=16},
    base_mem[cpu.PC], base_mem[cpu.PC+1], base_mem[cpu.PC+2], base_mem[cpu.PC+3]);
}

gb_parse_cartridge_header :: (cartridge: string) -> string, GB_Cartridge_Bank_Info {
  assert(cartridge.count > 0);
  GB_RAM_BANK_SIZES :: int.[0, 0, 8*1024, 32*1024, 128*1024, 64*1024];

  result: GB_Cartridge_Bank_Info;
  title_view: [] u8;
  title_view.data = cartridge.data + 0x0134;
  title_view.count = 16;
  title := to_string(title_view);
  cartridge_type := cartridge.data[0x0147];
  mbc: GB_Memory_Bank_Controller;
  if cartridge_type == {
    case 0x00; mbc = .ROM_ONLY;

    case 0x01; #through;
    case 0x02; #through;
    case 0x03; mbc = .MBC1;

    case 0x05; #through;
    case 0x06; mbc = .MBC2;
  }
  rom_size := 32 * 1024 * (1 << cartridge.data[0x0148]);
  ram_size := GB_RAM_BANK_SIZES[cartridge.data[0x0149]];

  result.mbc_type = mbc;
  result.rom_size = rom_size;
  result.ram_size = ram_size;
  return title, result;
}

gb_init :: () -> GB_State {
  gb_state: GB_State;

  // TODO: These should only be generated once and stored somewhere.
  gb_unprefixed_instrs, gb_cbprefixed_instrs := gb_load_instruction_data();
  gb_unprefixed_instr_vtbl, gb_cbprefixed_instr_vtbl := gb_generate_instruction_vtable();
  gb_state.unprefixed_instrs = gb_unprefixed_instrs;
  gb_state.cbprefixed_instrs = gb_cbprefixed_instrs;
  gb_state.unprefixed_instr_vtbl = gb_unprefixed_instr_vtbl;
  gb_state.cbprefixed_instr_vtbl = gb_cbprefixed_instr_vtbl;

  gb_state.base_mem = cast(*u8) alloc(0x10000);
  assert (gb_state.base_mem != null);

  test_rom, rom_load_result := read_entire_file(TEST_ROM_PATH);
  assert(rom_load_result);
  title, bank_info := gb_parse_cartridge_header(test_rom);
  log("Loaded ROM: %, with size: %, MBC: %, and SRAM: %", title, bank_info.rom_size, bank_info.mbc_type, bank_info.ram_size);
  memcpy(gb_state.base_mem, test_rom.data, 0x8000);

  gb_state.cartridge = cast([] u8) test_rom;
  gb_state.bank_info = bank_info;
  gb_state.active_bank = 1;

  gb_state.base_mem[0xFF44] = 0x90;
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

  return gb_state;
}

gb_step :: (gb: *GB_State) -> t_states: int {
  // NOTE: Fetch & execute next instruction
  instr: GB_Instruction;
  cycles_consumed := 1; // TODO: Verify this
  if !gb.low_power_mode {
    opcode := gb_read_pc(gb);
    if gb.halt_stunt_bug then gb.cpu.PC -= 1;
    gb.halt_stunt_bug = false;
    instr = gb.unprefixed_instrs[opcode];
    gb_instr := gb.unprefixed_instr_vtbl[opcode];
    if instr.mnemonic == .PREFIX {
      opcode = gb_read_pc(gb);
      instr = gb.cbprefixed_instrs[opcode];
      gb_instr = gb.cbprefixed_instr_vtbl[opcode];
    }
    cycles_consumed = gb_instr(gb);
    if cycles_consumed == 0 then cycles_consumed = instr.t_states;
    assert(cycles_consumed > 0);
  }

  // NOTE: Check for interrupts
  if instr.mnemonic != .EI && gb.ime_needs_update {
    gb.ime_needs_update = false;
    gb.cpu.IME = true;
  }
  pending_interrupts := gb_read(gb, 0xFF0F);
  enabled_interrupts := gb_read(gb, 0xFFFF);
  if gb.cpu.IME {
    interrupts := type_info(GB_Interrupt);
    for < interrupts.values {
      if (pending_interrupts & enabled_interrupts) & cast(u8) it {
        updated_reg := gb_read(gb, 0xFF0F);
        updated_reg &= ~cast(u8) it;
        gb_write(gb, 0xFF0F, updated_reg);
        gb.cpu.IME = false;
        gb_instr_push(gb, .PC, .NONE, 0);
        gb.cpu.PC = GB_INTERRUPT_SOURCE_LOOKUP[it_index];
        gb.low_power_mode = false;
      }
    }
  } else if gb.low_power_mode && (pending_interrupts & enabled_interrupts) {
    gb.low_power_mode = false;
  }

  // NOTE: Update timers
  gb.div_accum += cycles_consumed;
  if gb.div_accum >= GB_CLOCK_CYCLES_PER_DIV_INC {
    gb.div_accum = 0;
    gb.base_mem[0xFF04] += 1;
  }

  timer_control := gb_read(gb, 0xFF07);
  timer_enable := timer_control & 0x04;
  if timer_enable {
    clock_select := timer_control & 0x03;
    clock_cycles_per_inc := GB_CLOCK_CYCLES_PER_TIMER_INC[clock_select];
    gb.timer_accum += cycles_consumed;
    if gb.timer_accum >= clock_cycles_per_inc {
      gb.timer_accum = 0;
      tima := gb.base_mem[0xFF05];
      if tima == 0xFF {
        gb.base_mem[0xFF05] = gb.base_mem[0xFF06];
        gb_request_interrupt(gb, .TIMER);
      } else {
        gb.base_mem[0xFF05] += 1;
      }
    }
  }

  // NOTE: Test ROM logging
  if gb.base_mem[0xFF02] == 0x81 {
    out_char := gb.base_mem[0xFF01];
    print("%", to_string(*out_char,1));
    gb.base_mem[0xFF02] = 0x00;
  }

  return cycles_consumed;
}

#import "File";
#import "Bit_Operations";