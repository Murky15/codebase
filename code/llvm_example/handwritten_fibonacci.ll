;  A hand-written example of LLVM IR which encodes the fibonacci function in fibonacci.cpp.
;  The basics are covered here: https://www.youtube.com/watch?v=m8G_S5LwlTo
;  Relevant documentation can be found here: https://llvm.org/docs/LangRef.html
;
;  Compile with `llc handwritten_fibonacci.ll -filetype=obj` then `clang example.cpp handwritten_fibonacci.obj` and run
;
;  HINT: The contents of this file are meant to mirror, or be very similar to the *in-memory* IR representation
;  encoded by generate_fibonacci.cpp, and the internal IR representation of `fib` from fibonacci.cpp.

; Think of IR as platform-independent assembly

define i32 @fib (i32 %x) {
  entry:
    %cond = icmp sle i32 %x, 1                ; if (x <= 1)
    br i1 %cond, label %x_le_1, label %x_gt_1 ; branch to proper control-flow based on %cond
  x_le_1:
    ret i32 %x                                ; return x
  x_gt_1:
    ; IR has unlimited "virtual registers" which can be referenced %0,%1,%2... or named (see %x and %result)
    ; Any register (variable) can only be assigned to once
    ; Global symbols (like functions) are denoted by @ instead of %
    %0 = sub i32 %x, 1
    %1 = sub i32 %x, 2
    %3 = call i32 @fib(i32 %0)                ; fib(x-1)
    %4 = call i32 @fib(i32 %1)                ; fib(x-2)
    %result = add i32 %3, %4
    ret i32 %result
}