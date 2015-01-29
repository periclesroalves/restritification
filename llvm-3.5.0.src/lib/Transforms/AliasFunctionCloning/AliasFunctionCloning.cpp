/*=============================================================================
 *  Copyright (C) 2015 Victor Hugo Sperle Campos
 *===========================================================================*/

#include "AliasFunctionCloning.h"

// This code is based in Argument Promotion pass
// and Douglas' pass.
bool AliasFunctionCloning::runOnModule(Module &M)
{
	Module::iterator Mit, Mend;
	
	// check every function in module
	for (Mit = M.begin(), Mend = M.end(); Mit != Mend;
			++Mit) {
		Function &F = *Mit;

		FunctionType *FTy = F.getFunctionType();
		SmallVector<Type*, 4> Params;
		SmallVector<AttributeSet, 4> AttributesVec;

		const AttributeSet &PAL = F.getAttributes();

		// promote all pointer arguments to noalias
		unsigned ArgIndex = 1;
		Function::arg_iterator I, E;
		for (I = F.arg_begin(), E = F.arg_end(); I != E; ++I, ++ArgIndex) {
			Argument *A = I;

			Params.push_back(A->getType());
			AttributeSet attrs = PAL.getParamAttributes(ArgIndex);

			// argument is not pointer
			if (attrs.hasAttributes(ArgIndex)) {
				if (!A->getType()->isPointerTy()) {
					AttrBuilder B(attrs, ArgIndex);
					AttributesVec.push_back(AttributeSet::get(F.getContext(),
								Params.size(), B));
				}
				else {
					AttrBuilder B(attrs, ArgIndex);
					// add noalias tag
					B.addAttribute(Attribute::get(F.getContext(),
								Attribute::NoAlias));
					AttributesVec.push_back(AttributeSet::get(F.getContext(),
								Params.size(), B));
				}
			}
		}

		// Function attributes
		if (PAL.hasAttributes(AttributeSet::FunctionIndex)) {
			AttributesVec.push_back(AttributeSet::get(FTy->getContext(),
						PAL.getFnAttributes()));
		}

		// create our function clone
		Type *RTy = FTy->getReturnType();
		FunctionType *NFTy = FunctionType::get(RTy, Params, FTy->isVarArg());

		Function *newfunc = Function::Create(NFTy, F.getLinkage(), F.getName());
		newfunc->copyAttributesFrom(&F);
		newfunc->setAttributes(AttributeSet::get(F.getContext(), AttributesVec));

		ValueToValueMapTy VMap;
		SmallVector<ReturnInst*, 4> Returns;

		Function::arg_iterator NI = newfunc->arg_begin();
		for (I = F.arg_begin(), E = F.arg_end(); I != E; ++I, ++NI) {
			VMap[I] = NI;
		}

		CloneFunctionInto(newfunc, &F, VMap, false, Returns);

		F.getParent()->getFunctionList().insert(&F, newfunc);
		newfunc->setName(F.getName() + "_noalias");
	}

	return true;
}

char AliasFunctionCloning::ID = 0;
static RegisterPass<AliasFunctionCloning> X("afc",
		"Alias Function Cloning pass", false, false);

