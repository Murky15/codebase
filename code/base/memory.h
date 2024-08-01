#ifndef BASE_MEMORY_H
#define BASE_MEMORY_H

#if OS_MAC && ARCH_ARM64
# define PAGE_TABLE_SIZE Kilobytes(16)
#else
# define PAGE_TABLE_SIZE Kilobytes(4)
#endif

#define ARENA_DEFAULT_RESERVE_SIZE Gigabytes(8)
#define ARENA_DECOMMIT_THRESHOLD Megabytes(64)

typedef struct Arena {
  struct Arena *next;
  u64 pos;
  u64 commit_pos;
  u64 cap;
} Arena;

typedef struct Temp_Arena {
  Arena *arena;
  u64 pos;
} Temp_Arena;

// @note: Arena functions

core_function Arena*     arena_alloc(void);
core_function void       arena_release(Arena *arena);
core_function void*      arena_push_no_zero(Arena *arena, u64 size, u64 align);
core_function void*      arena_push(Arena *arena, u64 size, u64 align);
core_function void       arena_pop_to(Arena *arena, u64 pos);
core_function void       arena_pop(Arena *arena, u64 amount);
core_function void       arena_clear(Arena *arena);
core_function u64        arena_pos(Arena *arena);
#define arena_pushn(a,T,c) (T*)arena_push((a), sizeof(T) * (c), align_of(T));

// @note: Temp arena functions

core_function Temp_Arena temp_arena(Arena *arena);
core_function void       temp_arena_end(Temp_Arena temp);

// @note: Scratch arena

core_function Temp_Arena get_scratch(Arena **conflicts, u64 num_conflicts);
#define release_scratch(t) temp_arena_end(t)

#endif // BASE_MEMORY_H