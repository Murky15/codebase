#!/bin/bash

clang -IW:/code/third_party/llvm-project/llvm/include generate_fibonacci.cpp -LW:/code/third_party/llvm-project/build/Debug/lib -lLLVM-C -o gen_fib.exe