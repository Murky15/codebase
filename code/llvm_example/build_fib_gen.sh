llvm_opts=`llvm-config --system-libs --cxxflags --libs all`
cl -MD -EHsc -wd4624 ${llvm_opts//\//-} generate_fibonacci.cpp -Fegen_fib.exe
