/*============================================================================
 *  AliasFunctionCloning pass
 *
 *  Clones functions from the program and adds the noalias attribute whenever 
 *  possible.
 *
 *  Copyright (C) 2015 Victor Hugo Sperle Campos
 *
 *===========================================================================*/
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

namespace {
	class AliasFunctionCloning: public ModulePass {
		public:
		static char ID;

		AliasFunctionCloning():
			ModulePass(ID) {}

		bool runOnModule(Module &M);
	};
}

