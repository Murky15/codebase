#include <stdio.h>

// @note: Unity build

// Headers
#include "base/context.h"
#include "base/macros.h"
#include "base/types.h"
#include "base/memory.h"
#include "base/strings.h"

// Source
#include "base/memory.c"
#include "base/strings.c"

int
main (void) {
  print_context();
  return 0;
}