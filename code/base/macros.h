#ifndef BASE_MACROS_H
#define BASE_MACROS_H

// @todo: Sanitizing & Profiling helpers

//- @note: Constant macros

#if COMPILER_CL || COMPILER_TCC
# define threadvar __declspec(thread)
#elif COMPILER_CLANG || COMPILER_GCC
# define threadvar __thread
#else
# error "Thread local storage not implemented for this compiler!"
#endif

#if COMPILER_CL
# pragma section(".roglob", read)
# define read_only __declspec(allocate(".roglob"))
#else // @todo: Find a better way to do this
# define read_only const
#endif

#if LANG_CPP
# define zero_struct {}
#else
# define zero_struct {0}
#endif

// @todo: Handle shared library symbol exports
#if LANG_CPP
# define link extern "C"
# define link_shared link
#else
# define link extern
# define link_shared link
#endif

#define global static
#define local_persist static
#define function static
#define thread_shared static
#define fallthrough
#define core_function function // @note: For base functions so we can build a dll/lib

//- @note: Function macros
#define unused(v) (void)(v)
#define stmnt(s) do{ s } while (0)

#define stringify_(s) #s
#define stringify(s) stringify_(s)
#define glue_(a,b) a##b
#define glue(a,b) glue_(a,b)

#ifndef assert_break
# if OS_WINDOWS && (COMPILER_CL || COMPILER_CLANG)
#  define assert_break() __debugbreak()
# else
#  define assert_break() (*(volatile int*)0 = 0)
# endif
#endif

#if ENABLE_ASSERT
# define static_assert(c,id) typedef u8 glue(static_assert, id)[(c)?1:-1]
# define assert(c) stmnt( if(!(c)) { assert_break(); } )
#else
# define static_assert(c, id)
# define assert(c)
#endif

#if COMPILER_CL || COMPILER_TCC
# define alignof(x) __alignof(x)
#elif COMPILER_CLANG || COMPILER_GCC
# define alignof(x) __alignof__(x)
#else
# error "alignof not implemented!"
#endif

#if !LANG_CPP
# if COMPILER_CL // Every other compiler should define this correctly
#  define typeof(x) __typeof__(x)
# endif
#endif

#if LANG_CPP
# define comp_lit(T, ...) {__VA_ARGS__} // The one time MSVC gets picky about the standard
#else
# define comp_lit(T, ...) (T){__VA_ARGS__}
#endif
#define comp_zero(T) comp_lit(T, zero_struct)

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define clamp(a,min,max) ((a) < (min) ? (min) : (a) > (max) ? (max) : (a))
#define sign(x) (fabs(x)/(x))

#define array_count(a) (sizeof(a) / sizeof(a[0]))
#define swap(a, b) stmnt( typeof(a) __temp = a; a = b; b = __temp; )

#define be_to_le16(x) ((((u8*)(x))[0] << 8) | (((u8*)(x))[1]))
#define be_to_le32(x) ((((u8*)(x))[0] << 24) | (((u8*)(x))[1] << 16) | (((u8*)(x))[2] << 8) | (((u8*)(x))[3]))

#if ARCH_X64
# define reverse_byte(x) (((x) * 0x0202020202ULL & 0x010884422010ULL) % 1023)
#else
# error "No 32 bit support for byte reversal!"
#endif

#define check_bit(x,b) ((x)&(1<<(b)))
#define bit_mask(c) ((1<<(c))-1)
#define fourcc(x) *((u32*)x)

#define int_from_ptr(p) (u64)((void*)p)
#define ptr_from_int(i) (void*)(i)
#define member(T,m) (((T*)0)->m)
#define offset_member(T,m) int_from_ptr(&member(T,m))

#define is_pow_2(n) ((n) && !((n)&((n)-1)))
#define round_up_pow2(n,m) (((n)+(m-1))&-(m))
#define round_down_pow2(n,m) ((n)&-(m))

#define Kilobytes(n) ((u64)(n) << 10)
#define Megabytes(n) ((u64)(n) << 20)
#define Gigabytes(n) ((u64)(n) << 30)
#define Terabytes(n) ((u64)(n) << 401lu)

#define sqr(x) ((x)*(x))

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define byte_to_binary(byte)  \
((byte) & 0x80 ? '1' : '0'), \
((byte) & 0x40 ? '1' : '0'), \
((byte) & 0x20 ? '1' : '0'), \
((byte) & 0x10 ? '1' : '0'), \
((byte) & 0x08 ? '1' : '0'), \
((byte) & 0x04 ? '1' : '0'), \
((byte) & 0x02 ? '1' : '0'), \
((byte) & 0x01 ? '1' : '0')

//- @note: Memory operation wrappers

#include <string.h>
#define memory_zero(p,s) memset((p), 0, (s))
#define memory_init(p,s,v) memset((p), v, (s))
#define memory_match(a,b,s) (memcmp((a),(b),(s)) == 0)
#define memory_copy(d,s,sz) memmove((d),(s),(sz))

//- @note: Syntax helpers
#define ldefer(start, end) for(int _i_ = ((start), 0); _i_ == 0; (_i_ += 1, (end)))

// for...
#define each_in_list(i,lp) (typeof((lp)->first)(i)=(lp)->first;(i);(i)=(i)->next)
#define each_in_arrayc(i,a,c) (typeof(a)(i)=(a);((i)-(a))<(c);(i)+=1)
#define each_in_array(i,a) each_in_arrayc(i,a,array_count(a))
#define each_in_range(i,a,r) (typeof(a)(i)=&(a)[(r).first];((i)-(a))<(r).last;(i)+=1)

//- @note: Linked list macros

#define dll_push_back_np(f,l,n,next,prev) ((f)==0?\
((f)=(l)=(n),(n)->next=(n)->prev=0):\
((n)->prev=(l),(l)->next=(n),(l)=(n),(n)->next=0))
#define dll_push_back(f,l,n) dll_push_back_np(f,l,n,next,prev)

#define dll_push_front(f,l,n) dll_push_back_np(l,f,n,prev,next)

#define dll_remove_np(f,l,n,next,prev) ((f)==(n)?\
((f)==(l)?\
((f)=(l)=(0)):\
((f)=(f)->next,(f)->prev=0)):\
(l)==(n)?\
((l)=(l)->prev,(l)->next=0):\
((n)->next->prev=(n)->prev,\
(n)->prev->next=(n)->next))
#define dll_remove(f,l,n) dll_remove_np(f,l,n,next,prev)

#define sll_queue_push_n(f,l,n,next) (((f)==0?\
(f)=(l)=(n):\
((l)->next=(n),(l)=(n))),\
(n)->next=0)
#define sll_queue_push(f,l,n) sll_queue_push_n(f,l,n,next)

#define sll_queue_push_front_n(f,l,n,next) ((f)==0?\
((f)=(l)=(n),(n)->next=0):\
((n)->next=(f),(f)=(n)))
#define sll_queue_push_front(f,l,n) sll_queue_push_front_n(f,l,n,next)

#define sll_queue_pop_n(f,l,next) ((f)==(l)?\
(f)=(l)=0:\
((f)=(f)->next))
#define sll_queue_pop(f,l) sll_queue_pop_n(f,l,next)

#define sll_stack_push_n(f,n,next) ((n)->next=(f),(f)=(n))
#define sll_stack_push(f,n) sll_stack_push_n(f,n,next)

#define sll_stack_pop_n(f,next) ((f)==0?0:\
((f)=(f)->next))
#define sll_stack_pop(f) sll_stack_pop_n(f,next)

#endif // BASE_MACROS_H
