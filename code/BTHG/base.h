#ifndef BASE_H
#define BASE_H

// NOTE: Context

#if defined(_MSC_VER)
# define COMPILER_CL 1
# if defined(_WIN64) || defined(_WIN32)
#  define OS_WINDOWS 1
# else
#  error "Unsupported compiler/platform configuration"
# endif
#endif

#if defined(__arm__) || defined(_M_ARM)
# define ARCH_ARM32 1
#elif defined(__aarch64__) || defined(_M_ARM64)
# define ARCH_ARM64 1
#elif defined(__i386__) || defined(_M_IX86)
# define ARCH_X86 1
#elif defined(__amd64__) || defined(_M_AMD64)
# define ARCH_X64 1
#elif defined(__riscv)
# define ARCH_RISCV 1
#else
# error "Unsupported architecture target"
#endif

#if defined(__cplusplus)
# define LANG_CPP 1
#endif

#ifndef COMPILER_CL
# define COMPILER_CL 0
#endif

#ifndef OS_WINDOWS
# define OS_WINDOWS 0
#endif

#ifndef ARCH_ARM32
# define ARCH_ARM32 0
#endif
#ifndef ARCH_ARM64
# define ARCH_ARM64 0
#endif
#ifndef ARCH_X86
# define ARCH_X86 0
#endif
#ifndef ARCH_X64
# define ARCH_X64 0
#endif
#ifndef ARCH_RISCV
# define ARCH_RISCV 0
#endif

// NOTE: Helper macros

#ifndef base_function
# define base_function static
#endif
#define function       static
#define global         static
#define local_persist  static

#define Stmnt(s) do {s} while (0)

#if LANG_CPP
# define CompLit(T, ...) {__VA_ARGS__}
# define ZeroStruct(T) {}
#else
# define CompLit(T, ...) (T){__VA_ARGS__}
# define ZeroStruct(T) (T){0}
#endif

#ifndef AssertBreak
# define AssertBreak(...) (*(volatile int*)0x0 = 0)
#endif

#ifdef BASE_ENABLE_ASSERT
# define Assert(c, ...) Stmnt( if (!(c)) {AssertBreak(__VA_ARGS__);} )
# define StaticAssert(c,id) typedef int static_assert##id[(c)?1:-1]
#else
# define Assert(...)
# define StaticAssert(...)
#endif

#define ArrayCount(a) (sizeof(a)/sizeof((a)[0]))

#define IsPow2(n) ((n) > 0 && !((n)&((n)-1)))
#define RoundUpPow2(n,m) (((n)+((m)-1))&-(m))
#define RoundDownPow2(n,m) ((n)&-(m))

#define KB(n) ((u64)(n) << 10)
#define MB(n) ((u64)(n) << 20)
#define GB(n) ((u64)(n) << 30)
#define TB(n) ((u64)(n) << 401lu)

#ifndef Min
# define Min(a,b) ((a)<(b)?a:b)
#endif
#ifndef Max
# define Max(a,b) ((a)>(b)?a:b)
#endif
#ifndef Clamp
# define Clamp(x,min,max) Min(max,Max(min,x))
#endif

#if LANG_CPP
# include <type_traits>
# define TypeOf(T) std::remove_reference_t<decltype(T)>
#else
# define TypeOf(T) __typeof__(T)
#endif

#define EachInArrayPtrIt(p,c,it) (TypeOf(p)it=(p);(it-(p))<(c);it+=1)
#define EachInArrayPtr(p,c) EachInArrayPtrIt(p,c,it)
#define EachInArrayIt(p,c,it) (TypeOf((p)[0])*__idx=(p),it=*__idx;(__idx-(p))<(c);it=*(__idx+=1))
#define EachInArray(p,c) EachInArrayIt(p,c,it)
#define EachInStaticArrayPtrIt(a,it) (TypeOf(&(a)[0])it=(a);(it-(a))<ArrayCount(a);it+=1)
#define EachInStaticArrayPtr(a) EachInStaticArrayPtrIt(a,it)
#define EachInStaticArrayIt(a,it) (TypeOf((a)[0])*__idx=(a),it=*__idx;(__idx-(a))<ArrayCount(a);it=*(__idx+=1))
#define EachInStaticArray(a) EachInStaticArrayIt(a,it)

#define EachInListIt(L,it) (TypeOf((L).first)it=(L).first;it;it=it->next)
#define EachInList(L) EachInListIt(L,it)
#define EachInListPtrIt(L,it) (TypeOf((L)->first)it=(L)->first;it;it=it->next)
#define EachInListPtr(L) EachInListPtrIt(L,it)

#define DLLPushBackNP(f,l,n,next,prev) \
((f)==0?\
((f)=(l)=(n),(n)->next=(n)->prev=0):\
((n)->prev=(l),(l)->next=(n),(l)=(n),(n)->next=0))
#define DLLPushBack(f,l,n) DLLPushBackNP(f,l,n,next,prev)
#define DLLPushFront(f,l,n) DLLPushBackNP(l,f,n,prev,next)
#define DLLRemoveNP(f,l,n,next,prev) \
((f)==(n)?\
((f)==(l)?\
((f)=(l)=(0)):\
((f)=(f)->next,(f)->prev=0)):\
(l)==(n)?\
((l)=(l)->prev,(l)->next=0):\
((n)->next->prev=(n)->prev,\
(n)->prev->next=(n)->next))
#define DLLRemove(f,l,n) DLLRemoveNP(f,l,n,next,prev)
#define SLLQueuePushN(f,l,n,next) \
(((f)==0?\
(f)=(l)=(n):\
((l)->next=(n),(l)=(n))),\
(n)->next=0)
#define SLLQueuePush(f,l,n) SLLQueuePushN(f,l,n,next)
#define SLLQueuePushFrontN(f,l,n,next) \
((f)==0?\
((f)=(l)=(n),(n)->next=0):\
((n)->next=(f),(f)=(n)))
#define SLLQueuePushFront(f,l,n) SLLQueuePushFrontN(f,l,n,next)
#define SLLQueuePopN(f,l,next) \
((f)==(l)?\
(f)=(l)=0:\
((f)=(f)->next))
#define SLLQueuePop(f,l) SLLQueuePopN(f,l,next)
#define SLLStackPushN(f,n,next) ((n)->next=(f),(f)=(n))
#define SLLStackPush(f,n) SLLStackPushN(f,n,next)
#define SLLStackPopN(f,next) \
((f)==0?0:\
((f)=(f)->next))
#define SLLStackPop(f) SLLStackPopN(f,next)

#if LANG_CPP
extern "C" {
#endif

// NOTE: Types

#include <stdint.h>
#include <stdbool.h>
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float    f32;
typedef double   f64;

// NOTE: Memory

#define ARENA_DEFAULT_RESERVE_SIZE GB(1)

enum {
  ARENA_KIND_MMU,
  ARENA_KIND_CHAIN,
  ARENA_KIND_FIXED,

  ARENA_KIND_COUNT
};
typedef u32 Arena_Kind;

typedef struct Arena {
  Arena_Kind kind;
  u64 pos;
  u64 commit_pos;
  u64 cap;
  // TODO: Move these to save space?
  u64 commit_chunk_size;
  u64 decommit_granularity;
} Arena;

base_function Arena* arena_alloc (u64 cap, u64 commit_chunk_size, u64 decommit_granularity);
base_function Arena* arena_alloc_default (); // NOTE: Find and cache OS page table size
#define ArenaPush(a,T,c) arena_push_raw((a), sizeof(T)*(c), __alignof(T))
base_function void*  arena_push_raw (Arena *arena, u64 size, u64 align);
base_function u64    arena_pos (Arena *arena);
base_function void   arena_pop_to (Arena *arena, u64 pos);
base_function void   arena_pop_amt (Arena *arena, u64 amt);
base_function void   arena_reset (Arena *arena);
base_function void   arena_free (Arena *arena);

typedef struct Temp_Arena {
  Arena *arena;
  u64 restore_pos;
} Temp_Arena;

base_function Temp_Arena arena_temp_begin (Arena *arena);
base_function void       arena_temp_end (Temp_Arena temp);

#define NUM_SCRATCH_ARENAS 2
base_function Temp_Arena arena_scratch_get (Arena **conflicts, u64 num_conflicts);
#define ScratchBlock(c,n) for (Temp_Arena scratch=arena_scratch_get((c),(n)),__idx={0};__idx.restore_pos<1;__idx.restore_pos=(arena_temp_end(scratch),1))

// NOTE: Strings

#include <string.h>
#include <stdio.h>

typedef struct String8 {
  u8 *str;
  u64 count;
} String8, Str8;

typedef struct Str8_Node {
  struct Str8_Node *next;
  String8 string;
} Str8_Node;

typedef struct Str8_List {
  Str8_Node *first;
  Str8_Node *last;
  u64 num_nodes;
  u64 total_count;
} Str8_List;

typedef struct Str8_Join {
  String8 pre, sep, post;
} Str8_Join;

enum {
  MATCH_CASE_INSENSITIVE  = (1 << 0),
  MATCH_RIGHT_SIDE_SLOPPY = (1 << 1),
  MATCH_SLASH_INSENSITIVE = (1 << 2)
};
typedef u32 String_Match_Flags;

function inline bool
char_is_alpha_upper (u8 c) {
  return (c >= 65 && c <= 90);
}

function inline bool
char_is_alpha_lower (u8 c) {
  return (c >= 97 && c <= 122);
}

function inline bool
char_is_alpha (u8 c) {
  return char_is_alpha_upper(c) || char_is_alpha_lower(c);
}

function inline bool
char_is_digit (u8 c) {
  return (c >= 48 && c <= 57);
}

function inline bool
char_is_alpha_numeric (u8 c) {
  return char_is_alpha(c) || char_is_digit(c);
}

function inline bool
char_is_symbol (u8 c) {
  return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) || (c >= 123 && c < 127);
}

function inline bool
char_is_control (u8 c) {
  return (c <= 31);
}

function inline bool
char_is_space (u8 c) {
  return (c == 32) || (c == 127);
}

function inline u8
char_to_upper (u8 c) {
  return char_is_alpha_lower(c) ? c - 32 : c;
}

function inline u8
char_to_lower (u8 c) {
  return char_is_alpha_upper(c) ? c + 32 : c;
}

function inline u8
char_to_forward_slash (u8 c) {
  return c == '\\' ? '/' : c;
}

function inline u8
char_to_back_slash (u8 c) {
  return c == '/' ? '\\' : c;
}

base_function u64 cstr_length (const char *cstr);

base_function Str8 str8 (u8 *str, u64 count);
#define str8_cstring(cstr) str8((u8*)cstr, cstr_length(cstr))
#define str8_lit(s) str8((u8*)s, sizeof(s)-1)
base_function Str8 str8_range (u8 *first, u8 *opl);

// NOTE: %.*s in format string
#define str8_expand(s) (int)((s).count), (char*)((s).str)

base_function Str8 str8_sub (Str8 string, u64 first, u64 opl);
base_function Str8 str8_skip (Str8 string, u64 amount);
base_function Str8 str8_chop (Str8 string, u64 amount);
base_function Str8 str8_prefix (Str8 string, u64 size);
base_function Str8 str8_postfix (Str8 string, u64 size);

base_function bool str8_match (Str8 a, Str8 b, String_Match_Flags flags);
base_function u64  str8_find (Str8 haystack, Str8 needle, u64 start_pos, String_Match_Flags flags);

base_function Str8 str8_push_copy (Arena *arena, Str8 string);
base_function Str8 str8_pushfv (Arena *arena, char *fmt, va_list args);
base_function Str8 str8_pushf (Arena *arena, char *fmt, ...);

base_function void str8_list_push_node (Str8_List *list, Str8_Node *node);
base_function void str8_list_push_node_front (Str8_List *list, Str8_Node *node);
base_function void str8_list_push (Arena *arena, Str8_List *list, Str8 string);
base_function void str8_list_push_front (Arena *arena, Str8_List *list, Str8 string);
base_function void str8_list_pushf (Arena *arena, Str8_List *list, char *fmt, ...);
base_function void str8_list_concat (Str8_List *base, Str8_List *to_append);
base_function Str8_List str8_split (Arena *arena, Str8 string, u64 num_splitters, char *splits);
base_function Str8 str8_list_join (Arena *arena, Str8_List list, Str8_Join *opt_join_params);

base_function u8* str8_to_cstr (Arena *arena, Str8 string);
base_function u64 u64_from_str8 (Str8 string, u32 radix);
base_function s64 cint_from_str8 (Str8 string);
base_function f64 f64_from_str8 (Str8 string);

base_function u64 str8_hash (Str8 string);

// NOTE: Math

#include <math.h>

typedef union Vec2 {
  struct { f32 x, y; };
  struct { f32 min, max; };

  f32 e[2];
} Vec2, Range;

typedef union Vec3 {
  struct { f32 x, y, z; };
  struct { Vec2 xy; f32 z1; };
  struct { f32 x2; Vec2 yz; };

  f32 e[3];
} Vec3;

typedef union Vec4 {
  struct { f32 x, y, z, w; };
  struct { Vec2 xy; f32 z1, w1; };
  struct { f32 x2; Vec2 yz; f32 w2; };
  struct { f32 x3, y3; Vec2 zw; };
  struct { Vec3 xyz; f32 z4; };
  struct { f32 x5; Vec3 zyw; };

  f32 e[4];
} Vec4, Quat;

#define V2(...) CompLit(Vec2, __VA_ARGS__)
#define V3(...) CompLit(Vec3, __VA_ARGS__)
#define V4(...) CompLit(Vec4, __VA_ARGS__)

function inline Vec2
v2add (Vec2 a, Vec2 b) {
  return V2(a.x+b.x, a.y+b.y);
}

function inline Vec2
v2sub (Vec2 a, Vec2 b) {
  return V2(a.x-b.x,a.y-b.y);
}

function inline Vec2
v2mul (Vec2 a, Vec2 b) {
  return V2(a.x*b.x,a.y*b.y);
}

function inline Vec2
v2div (Vec2 a, Vec2 b) {
  return V2(a.x/b.x,a.y/b.y);
}

function inline Vec2
v2muls (Vec2 v, f32 s) {
  return V2(v.x*s,v.y*s);
}

function inline f32
v2len (Vec2 v) {
  return sqrtf(v.x*v.x + v.y*v.y);
}

function inline Vec2
v2norm (Vec2 v) {
  return v2muls(v, 1.f/v2len(v));
}

function inline f32
v2dist (Vec2 a, Vec2 b) {
  return v2len(v2sub(a,b));
}

function inline f32
v2dot (Vec2 a, Vec2 b) {
  return a.x*b.x + a.y*b.y;
}

function inline f32
v2angle (Vec2 a, Vec2 b) {
  f32 dot = v2dot(a, b);
  f32 mag = v2len(a)*v2len(b);

  return acosf(dot/mag);
}

function inline Vec3
v3add (Vec3 a, Vec3 b) {
  return V3(a.x+b.x,a.y+b.y,a.z+b.z);
}

function inline Vec3
v3sub (Vec3 a, Vec3 b) {
  return V3(a.x-b.x,a.y-b.y,a.z-b.z);
}

function inline Vec3
v3mul (Vec3 a, Vec3 b) {
  return V3(a.x*b.x,a.y*b.y,a.z*b.z);
}

function inline Vec3
v3div (Vec3 a, Vec3 b) {
  return V3(a.x/b.x,a.y/b.y,a.z/b.z);
}

function inline Vec3
v3muls (Vec3 v, f32 s) {
  return V3(v.x*s,v.y*s,v.z*s);
}

function inline f32
v3len (Vec3 v) {
  return sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

function inline Vec3
v3norm (Vec3 v) {
  return v3muls(v, 1.f/v3len(v));
}

function inline f32
v3dist (Vec3 a, Vec3 b) {
  return v3len(v3sub(a,b));
}

function inline f32
v3dot (Vec3 a, Vec3 b) {
  return a.x*b.x + a.y*b.y + a.z*b.z;
}

function inline Vec3
v3cross (Vec3 a, Vec3 b) {
  Vec3 r;
  r.x = a.y * b.z - a.z * b.y;
  r.y = a.z * b.x - a.x * b.z;
  r.z = a.x * b.y - a.y * b.x;

  return r;
}

#if LANG_CPP
}
#endif

#endif // BASE_H

#ifdef BASE_IMPLEMENTATION
#undef BASE_IMPLEMENTATION

#if OS_WINDOWS
# define __WIN32_MEM_RESERVE    0x00002000
# define __WIN32_MEM_COMMIT     0x00001000
# define __WIN32_MEM_DECOMMIT   0x00004000
# define __WIN32_MEM_RELEASE    0x00008000
# define __WIN32_PAGE_READWRITE 0x04

#if LANG_CPP
extern "C" {
#endif
#pragma comment(lib, "Kernel32")
#pragma comment(linker, "/alternatename:__win32_virtual_alloc=VirtualAlloc")
#pragma comment(linker, "/alternatename:__win32_virtual_free=VirtualFree")
void *__win32_virtual_alloc(void *lpAddress, size_t dwSize, u32 flAllocationType, u32 flProtect);
int   __win32_virtual_free(void *lpAddress, size_t dwSize, u32 dwFreeType);
#if LANG_CPP
}
#endif

# define Reserve(size) __win32_virtual_alloc(0, (size), __WIN32_MEM_RESERVE, __WIN32_PAGE_READWRITE)
# define Commit(addr, size) __win32_virtual_alloc((addr), (size), __WIN32_MEM_COMMIT, __WIN32_PAGE_READWRITE)
# define Decommit(addr, size) __win32_virtual_free((addr), (size), __WIN32_MEM_DECOMMIT)
# define Release(addr) __win32_virtual_free((addr), 0, __WIN32_MEM_RELEASE)
#else
# error "Missing memory backend implementation for this platform"
#endif

base_function Arena*
arena_alloc (u64 cap, u64 commit_chunk_size, u64 decommit_granularity) {
  Arena *arena = (Arena*) Reserve(cap);
  Commit(arena, commit_chunk_size);
  arena->kind = ARENA_KIND_MMU;
  arena->pos = sizeof(Arena);
  arena->commit_pos = commit_chunk_size;
  arena->cap = cap;
  arena->commit_chunk_size = commit_chunk_size;
  arena->decommit_granularity = decommit_granularity;

  return arena;
}

base_function Arena*
arena_alloc_default () {
  local_persist u64 commit_chunk_size = 0;
  local_persist u64 decommit_granularity = 0;

  if (commit_chunk_size == 0) {
#if OS_WINDOWS
# define OS_VM_PAGE_SIZE KB(64)
#endif
    commit_chunk_size = OS_VM_PAGE_SIZE;
    decommit_granularity = OS_VM_PAGE_SIZE * 2;
  }

  return arena_alloc(ARENA_DEFAULT_RESERVE_SIZE, commit_chunk_size, decommit_granularity);
}

base_function void*
arena_push_raw (Arena *arena, u64 size, u64 align) {
  Assert(IsPow2(align));
  u8 *base = (u8*)arena;
  u64 aligned_pos = RoundUpPow2(arena->pos, align);
  u64 new_pos = aligned_pos + size;
  Assert(new_pos <= arena->cap, "Arena %p is full!", arena);

  void *result = base + arena->pos;
  arena->pos = new_pos;
  if (new_pos > arena->commit_pos) {
    u64 commit_size = new_pos - arena->commit_pos;
    commit_size = RoundUpPow2(commit_size, arena->commit_chunk_size);
    Commit(base + arena->commit_pos, commit_size);
    arena->commit_pos += commit_size;
  }

  return result;
}

base_function u64
arena_pos (Arena *arena) {
  return arena->pos;
}

base_function void
arena_pop_to (Arena *arena, u64 pos) {
  pos = Clamp(pos, sizeof(Arena), arena->pos);
  arena->pos = pos;
  u64 nearest_commit = RoundUpPow2(arena->pos, arena->commit_chunk_size);
  u64 commit_diff = arena->commit_pos - nearest_commit;
  if (commit_diff >= arena->decommit_granularity) {
    Decommit((u8*)arena + nearest_commit, commit_diff);
    arena->commit_pos = nearest_commit;
  }
}

base_function void
arena_pop_amt (Arena *arena, u64 amt) {
  amt = Min(amt, arena->pos); // NOTE: This is so that overflows clear the arena rather than wrap around and do nothing
  arena_pop_to(arena, arena->pos - amt);
}

base_function void
arena_reset (Arena *arena) {
  arena_pop_to(arena, 0);
}

base_function void
arena_free (Arena *arena) {
  Release(arena);
}

base_function Temp_Arena
arena_temp_begin (Arena *arena) {
  return CompLit(Temp_Arena, arena, arena_pos(arena));
}

base_function void
arena_temp_end (Temp_Arena temp) {
  arena_pop_to(temp.arena, temp.restore_pos);
}

base_function Temp_Arena
arena_scratch_get (Arena **conflicts, u64 num_conflicts) {
  __declspec(thread) local_persist Arena *reservoir[NUM_SCRATCH_ARENAS];
  if (reservoir[0]->pos == 0) {
    for EachInStaticArrayPtr(reservoir) {
      *it = arena_alloc_default();
    }
  }

  for EachInStaticArrayIt(reservoir, scratch) {
    bool is_conflict = false;
    for EachInArrayIt(conflicts, num_conflicts, potential_conflict) {
      if (scratch == potential_conflict) {
        is_conflict = true;
        break;
      }
    }

    if (!is_conflict) return arena_temp_begin(scratch);
  }

  return ZeroStruct(Temp_Arena);
}

#if OS_WINDOWS
# undef __WIN32_MEM_RESERVE
# undef __WIN32_MEM_COMMIT
# undef __WIN32_MEM_DECOMMIT
# undef __WIN32_MEM_RELEASE
# undef __WIN32_PAGE_READWRITE
#endif

#undef Reserve
#undef Commit
#undef Decommit
#undef Release

base_function u64
cstr_length (const char *cstr) {
  const char *c;
  for (c=cstr;*c;c++);

  return c - cstr;
}

base_function Str8
str8 (u8 *str, u64 count) {
  return CompLit(Str8, str, count);
}

base_function Str8
str8_range (u8 *first, u8 *opl) {
  return str8(first, (u64)(opl-first));
}

base_function Str8
str8_sub (Str8 string, u64 first, u64 opl) {
  Str8 result = ZeroStruct(Str8);
  first = Min(first, string.count-1);
  opl = Min(opl, string.count);
  if (first < opl){
    result.str = string.str + first;
    result.count = (u64)(opl-first);
  }

  return result;
}

base_function Str8
str8_skip (Str8 string, u64 amount) {
  amount = Min(amount, string.count);

  return str8(string.str + amount, string.count - amount);
}

base_function Str8
str8_chop (Str8 string, u64 amount) {
  amount = Min(amount, string.count);

  return str8(string.str, string.count - amount);
}

base_function Str8
str8_prefix (Str8 string, u64 size) {
  size = Min(size, string.count);

  return str8(string.str, size);
}

base_function Str8
str8_postfix (Str8 string, u64 size) {
  size = Min(size, string.count);
  u64 skip = string.count - size;

  return str8(string.str + skip, size);
}

base_function bool
str8_match (Str8 a, Str8 b, String_Match_Flags flags) {
  bool result = false;
  if (a.count == b.count || flags & MATCH_RIGHT_SIDE_SLOPPY) {
    result = true;
    for (u64 i = 0; i < a.count; ++i) {
      u8 ca = a.str[i], cb = b.str[i];
      bool match = ca == cb;
      if (flags & MATCH_CASE_INSENSITIVE)
        match |= (char_to_lower(ca) == char_to_lower(cb));
      if (flags & MATCH_SLASH_INSENSITIVE)
        match |= (char_to_forward_slash(ca) == char_to_forward_slash(cb));

      if (match == false) {
        result = false;
        break;
      }
    }
  }

  return result;
}

base_function u64
str8_find (Str8 haystack, Str8 needle, u64 start_pos, String_Match_Flags flags) {
  u64 pos = 0;
  for (u64 i = start_pos; i < haystack.count; ++i) {
    if (needle.count > haystack.count - i) break;

    Str8 query = str8_sub(haystack, i, i + needle.count);
    if (str8_match(query, needle, flags)) {
      pos = i;
      break;
    }
  }

  return pos;
}

base_function Str8
str8_push_copy (Arena *arena, Str8 string) {
  u64 count = string.count;
  u8 *str = ArenaPush(arena, u8, count + 1);
  memcpy(str, string.str, count);

  return str8(str, count);
}

base_function Str8
str8_pushfv (Arena *arena, char *fmt, va_list args) {
  va_list backup_args;
  va_copy(backup_args, args);

  u64 try_size = KB(1);
  u8 *buff = ArenaPush(arena, u8, try_size);
  u64 actual_size = vsnprintf(buff, try_size, fmt, args);
  actual_size += 1;

  if (actual_size > try_size) {
    arena_pop_amt(arena, try_size);
    buff = ArenaPush(arena, u8, actual_size);
    vsnprintf(buff, actual_size, fmt, backup_args);
  } else {
    arena_pop_amt(arena, try_size - actual_size);
  }
  va_end(backup_args);

  return str8(buff, actual_size-1);
}

base_function Str8
str8_pushf (Arena *arena, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Str8 result = str8_pushfv(arena, fmt, args);
  va_end(args);

  return result;
}

base_function void
str8_list_push_node (Str8_List *list, Str8_Node *node) {
  SLLQueuePush(list->first, list->last, node);
  list->num_nodes += 1;
  list->total_count += node->string.count;
}

base_function void
str8_list_push_node_front (Str8_List *list, Str8_Node *node) {
  SLLQueuePushFront(list->first, list->last, node);
  list->num_nodes += 1;
  list->total_count += node->string.count;
}

base_function void
str8_list_push (Arena *arena, Str8_List *list, Str8 string) {
  Str8_Node *node = ArenaPush(arena, Str8_Node, 1);
  node->string = string;
  str8_list_push_node(list, node);
}

base_function void
str8_list_push_front (Arena *arena, Str8_List *list, Str8 string) {
  Str8_Node *node = ArenaPush(arena, Str8_Node, 1);
  node->string = string;
  str8_list_push_node_front(list, node);
}

base_function void
str8_list_pushf (Arena *arena, Str8_List *list, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Str8 result = str8_pushfv(arena, fmt, args);
  va_end(args);

  str8_list_push(arena, list, result);
}

base_function void
str8_list_concat (Str8_List *base, Str8_List *to_append) {
  if (to_append->first == 0) return;
  base->num_nodes += to_append->num_nodes;
  base->total_count += to_append->total_count;
  if (base->last == 0) {
    *base = *to_append;
  } else {
    base->last->next = to_append->first;
    base->last = to_append->last;
  }
}

base_function Str8_List
str8_split (Arena *arena, Str8 string, u64 num_splitters, char *splits) {
  Str8_List result = ZeroStruct(Str8_List);

  u8 *c = string.str;
  u64 last_split = 0;
  for (u64 i = 0; i < string.count; ++i,++c) {
    u64 cursor = c - string.str;
    for (u64 j = 0; j < num_splitters; ++j) {
      if (*c != splits[j]) continue;

      str8_list_push(arena, &result, str8_sub(string, last_split, cursor));
      last_split = cursor + 1;
      break;
    }
  }
  str8_list_push(arena, &result, str8_sub(string, last_split, string.count));

  return result;
}

base_function Str8
str8_list_join (Arena *arena, Str8_List list, Str8_Join *opt_join_params) {
  Str8_Join join_opts = opt_join_params != 0 ? *opt_join_params : ZeroStruct(Str8_Join);
  u64 total_size = join_opts.pre.count + list.total_count + join_opts.sep.count * (list.num_nodes-1) + join_opts.post.count;
  u8 *str = ArenaPush(arena, u8, total_size+1);
  memcpy(str, join_opts.pre.str, join_opts.pre.count);
  u64 offset = join_opts.pre.count;
  for EachInList(list) {
    if (offset > join_opts.pre.count) {
      memcpy(str + offset, join_opts.sep.str, join_opts.sep.count);
      offset += join_opts.sep.count;
    }
    memcpy(str + offset, it->string.str, it->string.count);
    offset += it->string.count;
  }
  memcpy(str + offset, join_opts.post.str, join_opts.post.count);

  return str8(str, total_size);
}

base_function u8*
str8_to_cstr (Arena *arena, Str8 string) {
  u8 *result = ArenaPush(arena, u8, string.count);
  memcpy(result, string.str, string.count);

  return result;
}

/*
base_function u64
u64_from_str8 (Str8 string, u32 radix) {

}

base_function s64
cint_from_str8 (Str8 string) {

}

base_function f64
f64_from_str8 (Str8 string) {

}

base_function u64
str8_hash (Str8 string) {

}
*/

#endif // BASE_IMPLEMENTATION