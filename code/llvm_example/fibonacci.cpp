/*
  This file serves as a "control" by implementing a simple fibonacci function in C++.
  Compile with `clang example.cpp fibonacci.cpp`
*/

extern "C" int fib (int x) {
    if (x <= 1)
        return x;
    return fib(x-1) + fib(x-2);
}