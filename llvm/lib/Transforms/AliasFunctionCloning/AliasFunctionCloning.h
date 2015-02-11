/*============================================================================
 *  AliasFunctionCloning pass
 *
 *  Clones functions from the program and adds the noalias attribute whenever 
 *  possible.
 *
 *  Copyright (C) 2015 Victor Hugo Sperle Campos
 *
 *===========================================================================*/
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CaptureTracking.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Support/CommandLine.h"
#include <algorithm>

using namespace llvm;

namespace {
class AliasFunctionCloning: public ModulePass {
public:
    static char ID;

    AliasFunctionCloning() :
            ModulePass(ID)
    {
    }

    void getAnalysisUsage(AnalysisUsage &AU) const;
    bool runOnModule(Module &M);
    void AddAliasScopeMetadata(ValueToValueMapTy &VMap, const DataLayout *DL,
            AliasAnalysis *AA, const Function *F, Function *NF);
};
}
