#pragma once

#include <vector>
#include <map>
#include <tuple>

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>

#include <llvm/Target/TargetMachine.h>

#include <llvm/Linker/Linker.h>