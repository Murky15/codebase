#ifndef BASE_MACROS_H
#define BASE_MACROS_H

// @todo: Sanitizing & Profiling helpers

// @note: Constant macros

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
#define fallthrough

// @note: Function macros

#define unused(v) (void)(v)

#define stmnt(s) do{ s } while (0)

#define stringify_(s) #s
#define stringify(s) stringify_(s)
#define glue_(a,b) a##b
#define glue(a,b) glue_(a,b)

#ifndef assert_break
# define assert_break() (*(volatile int*)0 = 0)
#endif

#if ENABLE_ASSERT
# define static_assert(c,id) typedef u8 glue(static_assert, id)[(c)?1:-1]
# define assert(c,msg) stmnt( if(!c) { assert_break(); } )
#else
# define static_assert(c, id)
# define assert(c)
#endif

// Because we are using c99
#if COMPILER_CL
# define align_of(x) __alignof(x)
#elif LLVM_LIKE_COMPILER
# define align_of(x) __alignof__(x)
#else
# error "align_of not implemented!"
#endif

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define clamp(a,min,max) ((a) < (min) ? (min) : (a) > (max) ? (max) : (a))

#define array_count(a) (sizeof(a) / sizeof(a[0]))
#define swap(T, a, b) stmnt( T __temp = a; a = b; b = __temp; )

#define int_from_ptr(p) (u64)((void*)p)
#define ptr_from_int(i) (void*)(i)
#define member(T,m) (((T*)0)->m)
#define offset_member(T,m) int_from_ptr(&member(T,m))

#define is_pow_2(n) ((n) && !((n)&((n)-1)))
#define round_up_pow2(n,m) (((n)+(m-1))&-(m))
#define round_down_pow2(n,m) ((n)&-(m))

#define Kilobytes(n) ((n) << 10)
#define Megabytes(n) ((n) << 20)
#define Gigabytes(n) ((n) << 30)
#define Terabytes(n) ((u64)(n) << 401lu)

// @note: Memory operation wrappers

#include <string.h>
#define memory_zero(p,s) memset((p), 0, (s))
#define memory_zero_struct(p) memory_zero((p), sizeof(*(p)))
#define memory_zero_array(a) memory_zero((a), sizeof(a))
#define memory_zero_typed(p,c) memory_zero((p), sizeof(*(p)*(c)))
#define memory_match(a,b,s) (memcmp((a),(b),(s)) == 0)
#define memory_copy(d,s,sz) memmove((d),(s),(sz))
#define memory_copy_struct(d,s) memory_copy((d),(s),min(sizeof(*(d)),sizeof(*(s))))
#define memory_copy_array(d,s) memory_copy((d),(s),min(sizeof(d),sizeof(s)))
#define memory_copy_typed(d,s,c) memory_copy((d),(s),min(sizeof(*(d)),sizeof(*(s)))*(c))

#define DeferLoop(start, end) for(int _i_ = ((start), 0); _i_ == 0; (_i_ += 1, (end)))

// @note: Linked list macros

#define DLLPushBack_NP(f,l,n,next,prev) ((f)==0?\
((f)=(l)=(n),(n)->next=(n)->prev=0):\
((n)->prev=(l),(l)->next=(n),(l)=(n),(n)->next=0))
#define DLLPushBack(f,l,n) DLLPushBack_NP(f,l,n,next,prev)

#define DLLPushFront(f,l,n) DLLPushBack_NP(l,f,n,prev,next)

#define DLLRemove_NP(f,l,n,next,prev) ((f)==(n)?\
((f)==(l)?\
((f)=(l)=(0)):\
((f)=(f)->next,(f)->prev=0)):\
(l)==(n)?\
((l)=(l)->prev,(l)->next=0):\
((n)->next->prev=(n)->prev,\
(n)->prev->next=(n)->next))
#define DLLRemove(f,l,n) DLLRemove_NP(f,l,n,next,prev)

#define SLLQueuePush_N(f,l,n,next) (((f)==0?\
(f)=(l)=(n):\
((l)->next=(n),(l)=(n))),\
(n)->next=0)
#define SLLQueuePush(f,l,n) SLLQueuePush_N(f,l,n,next)

#define SLLQueuePushFront_N(f,l,n,next) ((f)==0?\
((f)=(l)=(n),(n)->next=0):\
((n)->next=(f),(f)=(n)))
#define SLLQueuePushFront(f,l,n) SLLQueuePushFront_N(f,l,n,next)

#define SLLQueuePop_N(f,l,next) ((f)==(l)?\
(f)=(l)=0:\
((f)=(f)->next))
#define SLLQueuePop(f,l) SLLQueuePop_N(f,l,next)

#define SLLStackPush_N(f,n,next) ((n)->next=(f),(f)=(n))
#define SLLStackPush(f,n) SLLStackPush_N(f,n,next)

#define SLLStackPop_N(f,next) ((f)==0?0:\
((f)=(f)->next))
#define SLLStackPop(f) SLLStackPop_N(f,next)

#endif // BASE_MACROS_H
