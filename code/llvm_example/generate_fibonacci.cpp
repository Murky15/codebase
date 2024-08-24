#include <stdio.h>
#include "llvm-c/Core.h"

int main (int argc, char **argv) {
  unsigned major, minor, patch;
  LLVMGetVersion(&major, &minor, &patch);
  printf("At the time of writing the latest LLVM version is 20.0.0, your version is: %d.%d.%d\n", major, minor, patch);

  LLVMContextRef context = LLVMContextCreate();
  LLVMModuleRef  module  = LLVMModuleCreateWithNameInContext("generated_fibonacci", context);

  LLVMTypeRef i1  = LLVMInt1TypeInContext(context);
  LLVMTypeRef i32 = LLVMInt32TypeInContext(context);

  LLVMTypeRef  params[]      = { i32 };
  LLVMTypeRef  fib_func_type = LLVMFunctionType(i32, params, 1, false);
  LLVMValueRef fib_func      = LLVMAddFunction(module, "fib", fib_func_type);

  LLVMShutdown();
}