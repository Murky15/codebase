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
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"

using namespace llvm;

// Keep referring to doxygen info bc programmers manual is outdated
int main (int argc, char **argv) {
    // Create context & module (translation unit)
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
    
    // Entry block
    inst_builder.SetInsertPoint(entry);
    Value *if_result = inst_builder.CreateICmpSLE(x, ConstantInt::get(i32, 1));
    inst_builder.CreateCondBr(if_result, x_le_1_block, x_gt_1_block);
    
    // X less than 1 block
    inst_builder.SetInsertPoint(x_le_1_block);
    inst_builder.CreateRet(x);
    
    // X greater than 1 block
    inst_builder.SetInsertPoint(x_gt_1_block);
    Value *arg1 = inst_builder.CreateSub(x, ConstantInt::get(i32, 1));
    Value *arg2 = inst_builder.CreateSub(x, ConstantInt::get(i32, 2));
    CallInst *call1 = inst_builder.CreateCall(fib_func_type, fib_func, arg1);
    CallInst *call2 = inst_builder.CreateCall(fib_func_type, fib_func, arg2);
    Value *result = inst_builder.CreateAdd(call1, call2);
    inst_builder.CreateRet(result);
    
    // Print LLVM IR debug output
    outs() << "In-memory LLVM IR representation is:\n\n";
    module.print(outs(), nullptr);
    outs() << "\n";
    
    // Output object file
    // LLVM uses the legacy pass manager for CodeGen and the new
    // pass manager for optimizations
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();
    
    std::string target_triple = sys::getDefaultTargetTriple();
    std::string error;
    const Target *target = TargetRegistry::lookupTarget(target_triple, error);
    if (!target) {
        outs() << error;
        return 1;
    }
    
    TargetOptions opts;
    TargetMachine *machine = target->createTargetMachine(target_triple, "generic", "", opts, Reloc::PIC_);
    
    module.setDataLayout(machine->createDataLayout());
    module.setTargetTriple(target_triple);
    
    std::error_code ec;
    raw_fd_ostream output("generated_fibonacci.obj", ec, sys::fs::OF_None);
    if (ec) { 
        outs() << "Could not open file: " << ec.message();
        return 1;
    }
    
    legacy::PassManager output_pass;
    if (machine->addPassesToEmitFile(output_pass, output, nullptr, CodeGenFileType::ObjectFile)) {
        outs() << "Error emitting file";
        return 1;
    }
    output_pass.run(module);
    output.flush();
}