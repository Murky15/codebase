// @note: Arena functions

// @todo: Move this to an "os" codebase layer
#if OS_WINDOWS
# include <windows.h>
# define mem_reserve(size) VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE)
# define mem_commit(ptr, size) VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE)
# define mem_decommit(ptr, size) VirtualFree(ptr, size, MEM_DECOMMIT)
# define mem_release(ptr, size) VirtualFree(ptr, size, MEM_RELEASE)
#else
# error "Memory backend not implemented for this OS!"
#endif

static_assert(PAGE_TABLE_SIZE >= sizeof(Arena), check_arena_size);

link Arena*
arena_alloc (void) {
  Arena *result = 0;
  void *back_buffer = mem_reserve(ARENA_DEFAULT_RESERVE_SIZE);
  if (mem_commit(back_buffer, PAGE_TABLE_SIZE)) {
    result = (Arena*)back_buffer;
    result->next = 0;
    result->pos = sizeof(Arena);
    result->commit_pos = PAGE_TABLE_SIZE;
    result->cap = ARENA_DEFAULT_RESERVE_SIZE;
  }

  return result;
}

link void
arena_release (Arena *arena) {
  mem_release(arena, arena->cap);
}

link void*
arena_push_no_zero (Arena *arena, u64 size, u64 align) {
  assert(arena);
  void *result = 0;

  u8 *base = (u8*)arena;
  u64 aligned_pos = round_up_pow2(arena->pos, align);
  u64 new_pos = aligned_pos + size;
  if (new_pos <= arena->cap) {
    result = base + aligned_pos;
    arena->pos = new_pos;
    if (arena->commit_pos < new_pos) {
      u64 commit_size = new_pos - arena->commit_pos;
      commit_size = round_up_pow2(commit_size, PAGE_TABLE_SIZE);
      mem_commit(base + arena->commit_pos, commit_size);
      arena->commit_pos += commit_size;
    }
  } else {
    // @todo: Come up with better fallback strategies, right now just fail
    assert(0);
  }

  return result;
}

link void*
arena_push (Arena *arena, u64 size, u64 align) {
  void *result = arena_push_no_zero(arena, size, align);
  memory_zero(result, size);

  return result;
}

link void
arena_pop_to (Arena *arena, u64 pos) {
  assert(arena);
  u64 min_pos = sizeof(Arena);
  u64 max_pos = arena->pos;
  pos = clamp(pos, min_pos, max_pos);

  arena->pos = pos;
  u64 nearest_commit = round_up_pow2(pos, PAGE_TABLE_SIZE);
  if (nearest_commit + ARENA_DECOMMIT_THRESHOLD <= arena->commit_pos) {
    u64 commit_diff = arena->commit_pos - nearest_commit;
    mem_decommit((u8*)arena + nearest_commit, commit_diff);
    arena->commit_pos = nearest_commit;
  }
}

link void
arena_pop (Arena *arena, u64 amount) {
  u64 new_pos = arena->pos - amount;
  arena_pop_to(arena, new_pos);
}

link void
arena_clear (Arena *arena) {
  arena_pop_to(arena, 0);
}

link u64
arena_pos (Arena *arena) {
  return arena->pos;
}

// @note: Temp arena functions

link Temp_Arena
temp_arena (Arena *arena) {
  return (Temp_Arena){arena, arena->pos};
}

link void
temp_arena_end (Temp_Arena temp) {
  arena_pop_to(temp.arena, temp.pos);
}

// @note: Scratch arena


link Temp_Arena
get_scratch (Arena **conflicts, u64 num_conflicts) {
  threadvar local_persist Arena *scratch_pool[2];

  // First time init
  if (scratch_pool[0] == 0) {
    for (int i = 0; i < array_count(scratch_pool); ++i) {
      scratch_pool[i] = arena_alloc();
    }
  }

  // Find conflicting arena
  for (int i = 0; i < array_count(scratch_pool); ++i) {
    b32 is_conflict = 0;
    Arena *scratch = scratch_pool[i];
    for (int j = 0; j < num_conflicts; ++j) {
      Arena *p_conflict = conflicts[j];
      if (p_conflict == scratch) {
        is_conflict = 1;
        break;
      }
    }

    if (!is_conflict) {
      return temp_arena(scratch);
    }
  }

  return (Temp_Arena)zero_struct;
}