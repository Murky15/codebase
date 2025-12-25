core_function Wave_Data
wav_load (Arena *arena, String8 raw_wav_data) {
  Wave_Data result = comp_zero(Wave_Data);

  u8 *c = raw_wav_data.str;
  if (!str8_match(str8(c,4), str8_lit("RIFF"),0)) {
    printf("Error parsing WAV, invalid chunk format!\n");
    return result;
  }
  c += 8;
  if (!str8_match(str8(c,4), str8_lit("WAVE"),0)) {
    printf("Error parsing WAV, invalid chunk format!\n");
    return result;
  }
  c += 4;
  if (!str8_match(str8(c,4), str8_lit("fmt "),0)) {
    printf("Error parsing WAV, invalid chunk format!\n");
    return result;
  }
  c += 8;

  result.format = read_le16(c); c += 2;
  result.channels = read_le16(c); c += 2;
  result.frequency = read_le32(c); c += 4;
  result.bytes_per_sec = read_le32(c); c += 4;
  result.bytes_per_bloc = read_le16(c); c += 2;
  result.bits_per_sample = read_le16(c); c += 2;

  // NOTE: Loop over all other chunks, we don't care about them
  while (!str8_match(str8(c,4), str8_lit("data"),0)) {
    c += 4;
    u32 chunk_size = read_le32(c); c += 4;
    c += chunk_size;
  }
  c += 4;
  result.sample_buffer_size = read_le32(c); c += 4;
  result.sample_buffer = arena_pushn(arena, u8, result.sample_buffer_size);
  memory_copy(result.sample_buffer, c, result.sample_buffer_size);

  return result;
}
