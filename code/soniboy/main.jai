#import "Basic";
#import "File";
#import "Json";

CPU_Flags :: enum_flags {
  ZERO :: 0x80;
  SUBTRACTION :: 0x40;
  HALF_CARRY :: 0x20;
  CARRY :: 0x10;
}

CPU :: struct {
  union {
    af: u16;
    struct {
      a: u8;
      f: u8;
    }
  }
  union {
    bc: u16;
    struct {
      b: u8;
      c: u8;
    }
  }
  union {
    de: u16;
    struct {
      d: u8;
      e: u8;
    }
  }
  union {
    hl: u16;
    struct {
      h: u8;
      l: u8;
    }
  }
  sp: u16;
  pc: u16;
}

Json_Operand :: struct {
  name: string;
  immediate: bool;
  bytes: int;
}

Json_Opcode_Data :: struct {
  mnemonic: string;
  bytes: int;
  cycles: [] int;
  operands: [..] Json_CPU_Operand;
  immediate: bool;
  flags: Json_Value;
}

Json_Opcode :: struct {

}

Bit_Reader :: struct {
  buff: u64;
  count: int;
  cursor: *u8;
}

peek_bits :: (reader: *Bit_Reader, count: int) -> u64 {
  while reader.count < count {
    reader.buff |= cast(u64) reader.cursor.* << reader.count;
    reader.count += 8;
    reader.cursor += 1;
  }

  return reader.buff & (cast(u64)(1 << count) - 1);
}

consume_bits :: (reader: *Bit_Reader, count: int) -> u64 {
  result := peek_bits(reader, count);
  reader.buff >>= count;
  reader.count -= count;

  return result;
}

main :: () {
  test_rom, success := read_entire_file("W:/code/soniboy/test_roms/cpu_instrs.gb");
  assert (success);

  default_format_int := context.print_style.default_format_int;
  defer context.print_style.default_format_int = default_format_int;
  format_int := *context.print_style.default_format_int;
  format_int.base = 16;

  // NOTE: Skip over boot ROM straight into cartridge data
  test_rom.data += 0x0100;

  for * test_rom {
    opcode_reader := Bit_Reader.{cursor = it};
    /*
    third  := consume_bits(*opcode_reader, 3);
    second := consume_bits(*opcode_reader, 3);
    first  := consume_bits(*opcode_reader, 2);
    */
    print("% ", it.*);
  }
}