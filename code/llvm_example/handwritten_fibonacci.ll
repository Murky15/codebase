; A handwritten fib implementation in LLVM IR
; For installing LLVM, see: https://llvm.org/docs/GettingStarted.html#getting-the-source-code-and-building-llvm

; Compile with `llc handwritten_fibonacci.ll -filetype=obj` then `clang example.cpp handwritten_fibonacci.obj`
; (hint: you can use `clang -O0 -emit-llvm fibonacci.cpp` to see what the compiler would've done)

; I recommend watching this talk for the fundementals: https://www.youtube.com/watch?v=m8G_S5LwlTo
; Relevant documentation and a comprehensive list of instructions can be found here: https://llvm.org/docs/LangRef.html

; Think of IR as platform-independent assembly

; Define function `fib` in module scope
define i32 @fib (i32 %x) {
  entry:
    %cond = icmp sle i32 %x, 1                ; if (x <= 1)
    br i1 %cond, label %x_le_1, label %x_gt_1 ; branch to proper control-flow based on %cond
  x_le_1:
    ret i32 %x                                ; return x
  x_gt_1:
    ; IR has unlimited "virtual registers" which can be identified %0,%1,%2... or named (see %x and %result)
    ; Any identifier can only be assigned to once
    ; Global symbols (like functions) are prefixed with @ instead of %
    %0 = sub i32 %x, 1
    %1 = sub i32 %x, 2
    %3 = call i32 @fib(i32 %0)                ; fib(x-1)
    %4 = call i32 @fib(i32 %1)                ; fib(x-2)
    %result = add i32 %3, %4
    ret i32 %result
}