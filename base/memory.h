#ifndef BASE_MEMORY_H
#define BASE_MEMORY_H

#if OS_MAC && ARCH_ARM64
# define PAGE_TABLE_SIZE Kilobytes(16)
#else
# define PAGE_TABLE_SIZE Kilobytes(4)
#endif

typedef u32 Arena_Backend_Flags;
enum {
  ARENA_BACKEND_FIXED = (1 << 0),
  ARENA_BACKEND_CHAINED = (1 << 1),
  ARENA_BACKEND_MMU = (1 << 2),
};

typedef struct Arena {
  Arena_Backend_Flags backend;
  struct Arena *next;
  u64 pos;
  u64 cap;
} Arena;

typedef struct Temp_Arena {
  Arena *arena;
  u64 pos;
} Temp_Arena;

// @note: Arena functions

link Arena*     arena_alloc(Arena_Backend_Flags flags);
link void       arena_release(Arena *arena);
link void*      arena_push_no_zero(Arena *arena, u64 size, u64 align);
link void*      arena_push(Arena *arena, u64 size, u64 align);
link void       arena_pop_to(Arena *arena, u64 pos);
link void       arena_pop(Arena *arena, u64 size);
link void       arena_clear(Arena *arena);
link void       arena_pos(Arena *arena);
#define arena_push_n(a,T,c) arena_push((a), sizeof(T) * (c), align_of(T));

// @note: Temp arena functions

link Temp_Arena temp_arena(Arena *arena);
link void       temp_arena_end(Temp_Arena temp);

#endif // BASE_MEMORY_H