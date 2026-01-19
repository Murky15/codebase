#import "Basic";
#import "File";
#import "Json";

#if OS == .WINDOWS {
  TEST_ROM_PATH :: "W:/code/soniboy/test_roms/cpu_instrs.gb";
  OPCODE_DESCRIPTOR_PATH :: "W:/code/soniboy/gb_opcodes.json";
} else #if OS == .LINUX {
  TEST_ROM_PATH :: "/home/gmb/Workspace/root/code/soniboy/test_roms/cpu_instrs.gb";
  OPCODE_DESCRIPTOR_PATH :: "/home/gmb/Workspace/root/code/soniboy/gb_opcodes.json";
}

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

CPU_Operand :: struct {
  name: string;
  immediate: bool;
  bytes: int;
}

CPU_Opcode_Data :: struct {
  mnemonic: string;
  bytes: int;
  cycles: [] int;
  operands: [] CPU_Operand;
  immediate: bool;
  flags: *JSON_Value;
}

CPU_Opcode_Table :: struct #type_info_no_size_complaint {
  #insert #run () -> string {
    #import "String";
    builder: String_Builder;
    for 0x0..0xFF {
      format := FormatInt.{base=16, value=it, minimum_digits=2};
      hex := sprint("%", format);
      to_upper_in_place(hex);
      print_to_builder(*builder, "_0x%: CPU_Opcode_Data; @JsonName(0x%)\n", hex, hex);
    }

    return builder_to_string(*builder);
  }();
  #overlay (_0x00) opcodes: [0x100] CPU_Opcode_Data;
}

CPU_Instruction_Set :: struct {
  unprefixed: CPU_Opcode_Table;
  cbprefixed: CPU_Opcode_Table;
};

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
  cpu_load_result, cpu_instrs := json_parse_file(OPCODE_DESCRIPTOR_PATH, CPU_Instruction_Set);
  assert (cpu_load_result);

  test_rom, rom_load_result := read_entire_file(TEST_ROM_PATH);
  assert (rom_load_result);

  default_format_int := context.print_style.default_format_int;
  defer context.print_style.default_format_int = default_format_int;
  format_int := *context.print_style.default_format_int;
  format_int.base = 16;

  // NOTE: Skip over boot ROM straight into cartridge data
  rom_reader := Bit_Reader.{cursor = test_rom.data + 0x0100};

  while rom_reader.cursor - test_rom.data < test_rom.count {
    opcode := rom_reader.cursor.*;
    op_data := cpu_instrs.unprefixed.opcodes[opcode];
    third  := consume_bits(*rom_reader, 3);
    second := consume_bits(*rom_reader, 3);
    first  := consume_bits(*rom_reader, 2);
  }
}