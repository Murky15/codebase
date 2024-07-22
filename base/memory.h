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

link Arena*     arena_alloc(void);
link void       arena_release(Arena *arena);
link void*      arena_push_no_zero(Arena *arena, u64 size, u64 align);
link void*      arena_push(Arena *arena, u64 size, u64 align);
link void       arena_pop_to(Arena *arena, u64 pos);
link void       arena_pop(Arena *arena, u64 amount);
link void       arena_clear(Arena *arena);
link u64        arena_pos(Arena *arena);
#define arena_push_n(a,T,c) arena_push((a), sizeof(T) * (c), align_of(T));

// @note: Temp arena functions

link Temp_Arena temp_arena(Arena *arena);
link void       temp_arena_end(Temp_Arena temp);

// @note: Scratch arena

link Temp_Arena get_scratch(Arena **conflicts, u64 num_conflicts);
#define release_scratch(t) temp_arena_end(t)

#endif // BASE_MEMORY_H