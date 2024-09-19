; A handwritten fib implementation in LLVM IR
; Compile with `llc handwritten_fibonacci.ll -filetype=obj` then `clang main.cpp handwritten_fibonacci.obj`

; Define function `fib` in module scope
define i32 @fib (i32 %x) {
  ; entry, x_le_1, and x_gt_1 are called "basic blocks",
  ; they represent a sequence of un-interrupted instructions
  ; and have a clear beginning and end. They are terminated
  ; by special instructions like `ret` or `br`
  entry:                                      
    %cond = icmp sle i32 %x, 1                ; if (x <= 1)
    br i1 %cond, label %x_le_1, label %x_gt_1 ; control-flow based on %cond
  x_le_1:
    ret i32 %x                                ; return x
  x_gt_1:
    ; IR has unlimited "virtual registers" which 
    ; can be identified %0,%1,%2... or named (see %x and %result)
    ; Any identifier can only be assigned to once
    ; Global symbols (like functions) are prefixed with @ instead of %
    %0 = sub i32 %x, 1
    %1 = sub i32 %x, 2
    %3 = call i32 @fib(i32 %0)                ; fib(x-1)
    %4 = call i32 @fib(i32 %1)                ; fib(x-2)
    %result = add i32 %3, %4
    ret i32 %result
}