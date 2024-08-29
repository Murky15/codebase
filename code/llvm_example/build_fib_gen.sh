llvm_opts=`llvm-config --system-libs --cxxflags --libs all`
mkdir -p build
pushd build >> /dev/null
cl -MD -wd4624 ${llvm_opts//\//-} -EHsc ../generate_fibonacci.cpp -Fegen_fib.exe
popd >> /dev/null