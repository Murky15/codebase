#ifndef WAV_H
#define WAV_H

typedef struct Wave_Data {
  u16 format;
  u16 channels;
  u32 frequency;
  u32 bytes_per_sec;
  u16 bytes_per_bloc;
  u16 bits_per_sample;

  u8 *sample_buffer;
  u64 sample_buffer_size;
} Wave_Data;

core_function Wave_Data wav_load(Arena *arena, String8 raw_wav_data);

#endif // WAV_H