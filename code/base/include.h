#ifndef BASE_INCLUDE_H
#define BASE_INCLUDE_H

#include <stdio.h>

#include "context.h"

#if COMPILER_CL
# include <intrin.h>
#else
# error "Intrinsics headers not supplied for this compiler!
#endif

#include "macros.h"
#include "types.h"
#include "memory.h"
#include "strings.h"

#endif // BASE_INCLUDE_H