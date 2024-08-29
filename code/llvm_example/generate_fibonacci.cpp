// The rule for includes is just to include whatever you use, looking at this: https://llvm.org/doxygen/ 
// is somewhat helpful to keep track of what exists where

#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Config/llvm-config.h>

/*
    This program generates an object file that defines the `fib` funciton using the LLVM API
A good point of reference is the LLVM programming manual, specifically this section: https://llvm.org/docs/ProgrammersManual.html#helpful-hints-for-common-operations and everything after
 Everything else is just a lot of C++ cruft

 To compile, first execute `llvm-config` to gain context about the environment (includes, libs, flags, etc)
 You may need to play around, but what works for me is: `llvm-config --cxxflags --system-libs --libs all`
Pass the output of that to your compiler, e.g: `cl -MD $llvm_info generate_fibonacci.cpp -Fegen_fib.exe` (on MSVC)
After that `clang example.cpp generated_fibonacci.obj`

If you want to use cmake instead, look here: https://llvm.org/docs/UserGuides.html#llvm-builds-and-distributions

*BEWARE* the programming manual is *not* up to date so remember to periodically check back to the doxygen
    */

using namespace llvm;

int main (int argc, char **argv) {
    // If there is a great disparity, beware!
    outs() << "At the time of writing, LLVM version is 20.0.0. Your version is: " << LLVM_VERSION_MAJOR << "." << LLVM_VERSION_MINOR << "." << LLVM_VERSION_PATCH << "\n";
    
    // Create context & module (Think of a module as like a translation unit)
    LLVMContext context;
    Module module("generated_fibonacci", context);
    
    // Obtain handles to basic types
    IntegerType *i32 = Type::getInt32Ty(context);
    
    // Create fib function and functon blocks
    ArrayRef<Type*> params((Type*)i32);
    FunctionType *fib_func_type = FunctionType::get(i32, params, false);
    Function *fib_func = Function::Create(fib_func_type, GlobalValue::ExternalLinkage, "fib", module);
    
    BasicBlock *entry = BasicBlock::Create(context, "entry", fib_func);
    BasicBlock *x_le_1_block = BasicBlock::Create(context, "x_le_1", fib_func);
    BasicBlock *x_gt_1_block = BasicBlock::Create(context, "x_gt_1", fib_func);
    
    // Populate functon instructions
    IRBuilder<> inst_builder(context); 
    Value *x = (Value*)fib_func->getArg(0);
    
    // Entry
    inst_builder.SetInsertPoint(entry);
    Value *if_result = inst_builder.CreateICmpSLE(x, ConstantInt::get(i32, 1));
    inst_builder.CreateCondBr(if_result, x_le_1_block, x_gt_1_block);
    
    // X less than 1
    inst_builder.SetInsertPoint(x_le_1_block);
    inst_builder.CreateRet(x);
    
    // X greater than 1
    inst_builder.SetInsertPoint(x_gt_1_block);
    Value *arg1   = inst_builder.CreateSub(x, ConstantInt::get(i32, 1));
    Value *arg2   = inst_builder.CreateSub(x, ConstantInt::get(i32, 2));
    Value *call1  = inst_builder.CreateCall(fib_func_type, fib_func, arg1);
    Value *call2  = inst_builder.CreateCall(fib_func_type, fib_func, arg2);
    Value *result = inst_builder.CreateAdd(call1, call2);
    inst_builder.CreateRet(result);
    
    // Print LLVM IR debug output
    outs() << "--In-memory LLVM IR representation is:--\n";
    module.print(outs(), nullptr);
    outs() << "\n";
    
    /*
  Output object file

LLVM uses the legacy pass manager for CodeGen and the new pass manager for optimizations
More info: https://llvm.org/docs/WritingAnLLVMNewPMPass.html , https://llvm.org/docs/CodeGenerator.html , https://llvm.org/docs/UserGuides.html#optimizations
    */
    
    // You may want to do something more specific for your project, this is just for your convenience when compiling
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();
    
    std::string target_triple = sys::getDefaultTargetTriple();
    std::string error;
    const Target *target = TargetRegistry::lookupTarget(target_triple, error);
    if (!target) {
        errs() << error;
        return 1;
    }
    TargetOptions opts;
    TargetMachine *machine = target->createTargetMachine(target_triple, "generic", "", opts, Reloc::PIC_);
    
    // LLVM recommends this as it allows for more platform-specific optimizations: https://llvm.org/docs/Frontend/PerformanceTips.html
    module.setDataLayout(machine->createDataLayout());
    module.setTargetTriple(target_triple);
    
    std::error_code ec;
    raw_fd_ostream output_stream("generated_fibonacci.obj", ec, sys::fs::OF_None);
    if (ec) { 
        errs() << "Could not open file: " << ec.message();
        return 1;
    }
    
    legacy::PassManager output_pass;
    if (machine->addPassesToEmitFile(output_pass, output_stream, nullptr, CodeGenFileType::ObjectFile)) {
        errs() << "Error emitting file";
        return 1;
    }
    output_pass.run(module);
    output_stream.flush();
    
    /*
Hopefully you are now able to envision how you can reuse these LLVM constructs to generate arbitrary code
Thanks for reading!
*/
    
    return 0;
}