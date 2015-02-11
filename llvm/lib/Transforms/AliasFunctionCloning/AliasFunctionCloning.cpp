/*=============================================================================
 *  Copyright (C) 2015 Victor Hugo Sperle Campos
 *===========================================================================*/

#define DEBUG_TYPE "afc"

#include "AliasFunctionCloning.h"

STATISTIC(NumClonedFuncs, "Number of cloned functions");

void AliasFunctionCloning::getAnalysisUsage(AnalysisUsage &AU) const
{
    AU.setPreservesCFG();
    AU.addRequired<AliasAnalysis>();
    AU.addRequired<DataLayoutPass>();
}

// This code is based in Argument Promotion pass
// and Douglas' pass.
bool AliasFunctionCloning::runOnModule(Module &M)
{
    Module::iterator Mit, Mend;

    AliasAnalysis *AA = &getAnalysis<AliasAnalysis>();
    const DataLayout *DL = &getAnalysis<DataLayoutPass>().getDataLayout();

    // check every function in module
    for (Mit = M.begin(), Mend = M.end(); Mit != Mend; ++Mit) {
        Function &F = *Mit;

        if (F.isDeclaration())
            continue;

        //errs() << F.getName() << "\n";

        Function::arg_iterator I, E;

        // first we check if this function has any pointer arguments at all
        bool anyPointerArg = false;
        for (I = F.arg_begin(), E = F.arg_end(); I != E; ++I) {
            const Argument *A = I;
            if (A->getType()->isPointerTy()) {
                anyPointerArg = true;
                break;
            }
        }

        if (anyPointerArg == false) {
            continue;
        }

        FunctionType *FTy = F.getFunctionType();
        SmallVector<Type*, 4> Params;
        SmallVector<AttributeSet, 4> AttributesVec;

        const AttributeSet &PAL = F.getAttributes();

        // promote all pointer arguments to noalias
        unsigned ArgIndex = 1;

        for (I = F.arg_begin(), E = F.arg_end(); I != E; ++I, ++ArgIndex) {
            Argument *A = I;

            Params.push_back(A->getType());
            AttributeSet attrs = PAL.getParamAttributes(ArgIndex);

//			if (attrs.hasAttributes(ArgIndex)) {
            // argument is not pointer
            if (!A->getType()->isPointerTy()) {
                AttrBuilder B(attrs, ArgIndex);
                AttributesVec.push_back(
                        AttributeSet::get(F.getContext(), Params.size(), B));
            }
            else {
                AttrBuilder B(attrs, ArgIndex);
                // add noalias tag
                B.addAttribute(
                        Attribute::get(F.getContext(), Attribute::NoAlias));
                AttributesVec.push_back(
                        AttributeSet::get(F.getContext(), Params.size(), B));
            }
//			}
        }

        // Function attributes
        if (PAL.hasAttributes(AttributeSet::FunctionIndex)) {
            AttributesVec.push_back(
                    AttributeSet::get(FTy->getContext(),
                            PAL.getFnAttributes()));
        }

        // create our function clone
        Type *RTy = FTy->getReturnType();
        FunctionType *NFTy = FunctionType::get(RTy, Params, FTy->isVarArg());

        Function *newfunc = Function::Create(NFTy, F.getLinkage(), F.getName());
        newfunc->copyAttributesFrom(&F);
        newfunc->setAttributes(
                AttributeSet::get(F.getContext(), AttributesVec));

        ValueToValueMapTy VMap;
        SmallVector<ReturnInst*, 4> Returns;

        Function::arg_iterator NI = newfunc->arg_begin();
        for (I = F.arg_begin(), E = F.arg_end(); I != E; ++I, ++NI) {
            VMap[I] = NI;
        }

        CloneFunctionInto(newfunc, &F, VMap, false, Returns);

        F.getParent()->getFunctionList().insert(&F, newfunc);
        newfunc->setName(F.getName() + "_noalias");

        AddAliasScopeMetadata(VMap, DL, AA, &F, newfunc);

        ++NumClonedFuncs;
    }

    return true;
}

/// Taken almost entirely from Transforms/Utils/InlineFunction.cpp
void AliasFunctionCloning::AddAliasScopeMetadata(ValueToValueMapTy &VMap,
        const DataLayout *DL, AliasAnalysis *AA, const Function *F,
        Function *NF)
{
    const Function *CalledFunc = NF;
    SmallVector<const Argument *, 4> FunArgs;

    for (Function::const_arg_iterator I = CalledFunc->arg_begin(), E =
            CalledFunc->arg_end(); I != E; ++I) {
        if (!I->hasNUses(0))
            FunArgs.push_back(I);
    }

    if (FunArgs.empty())
        return;

    // To do a good job, if a noalias variable is captured, we need to know if
    // the capture point dominates the particular use we're considering.
    DominatorTree DT;
    DT.recalculate(const_cast<Function&>(*CalledFunc));

    // noalias indicates that pointer values based on the argument do not alias
    // pointer values which are not based on it. So we add a new "scope" for each
    // noalias function argument. Accesses using pointers based on that argument
    // become part of that alias scope, accesses using pointers not based on that
    // argument are tagged as noalias with that scope.

    DenseMap<const Argument *, MDNode *> NewScopes;
    MDBuilder MDB(CalledFunc->getContext());

    // Create a new scope domain for this function.
    MDNode *NewDomain = MDB.createAnonymousAliasScopeDomain(
            CalledFunc->getName());
    for (unsigned i = 0, e = FunArgs.size(); i != e; ++i) {
        const Argument *A = FunArgs[i];

        std::string Name = CalledFunc->getName();
        if (A->hasName()) {
            Name += ": %";
            Name += A->getName();
        }
        else {
            Name += ": argument ";
            Name += utostr(i);
        }

        // Note: We always create a new anonymous root here. This is true regardless
        // of the linkage of the callee because the aliasing "scope" is not just a
        // property of the callee, but also all control dependencies in the caller.
        MDNode *NewScope = MDB.createAnonymousAliasScope(NewDomain, Name);
        NewScopes.insert(std::make_pair(A, NewScope));
    }

    // Iterate over all new instructions in the map; for all memory-access
    // instructions, add the alias scope metadata.
    for (ValueToValueMapTy::iterator VMI = VMap.begin(), VMIE = VMap.end();
            VMI != VMIE; ++VMI) {
        if (const Instruction *I = dyn_cast<Instruction>(VMI->first)) {
            if (!VMI->second)
                continue;

            Instruction *NI = dyn_cast<Instruction>(VMI->second);
            if (!NI)
                continue;

            bool IsArgMemOnlyCall = false, IsFuncCall = false;
            SmallVector<const Value *, 2> PtrArgs;

            if (const LoadInst *LI = dyn_cast<LoadInst>(I))
                PtrArgs.push_back(LI->getPointerOperand());
            else if (const StoreInst *SI = dyn_cast<StoreInst>(I))
                PtrArgs.push_back(SI->getPointerOperand());
            else if (const VAArgInst *VAAI = dyn_cast<VAArgInst>(I))
                PtrArgs.push_back(VAAI->getPointerOperand());
            else if (const AtomicCmpXchgInst *CXI = dyn_cast<AtomicCmpXchgInst>(
                    I))
                PtrArgs.push_back(CXI->getPointerOperand());
            else if (const AtomicRMWInst *RMWI = dyn_cast<AtomicRMWInst>(I))
                PtrArgs.push_back(RMWI->getPointerOperand());
            else if (ImmutableCallSite ICS = ImmutableCallSite(I)) {
                // If we know that the call does not access memory, then we'll still
                // know that about the inlined clone of this call site, and we don't
                // need to add metadata.
                if (ICS.doesNotAccessMemory())
                    continue;

                IsFuncCall = true;
                if (AA) {
                    AliasAnalysis::ModRefBehavior MRB = AA->getModRefBehavior(
                            ICS);
                    if (MRB == AliasAnalysis::OnlyAccessesArgumentPointees
                            || MRB == AliasAnalysis::OnlyReadsArgumentPointees)
                        IsArgMemOnlyCall = true;
                }

                for (ImmutableCallSite::arg_iterator AI = ICS.arg_begin(), AE =
                        ICS.arg_end(); AI != AE; ++AI) {
                    // We need to check the underlying objects of all arguments, not just
                    // the pointer arguments, because we might be passing pointers as
                    // integers, etc.
                    // However, if we know that the call only accesses pointer arguments,
                    // then we only need to check the pointer arguments.
                    if (IsArgMemOnlyCall && !(*AI)->getType()->isPointerTy())
                        continue;

                    PtrArgs.push_back(*AI);
                }
            }

            // If we found no pointers, then this instruction is not suitable for
            // pairing with an instruction to receive aliasing metadata.
            // However, if this is a call, this we might just alias with none of the
            // noalias arguments.
            if (PtrArgs.empty() && !IsFuncCall)
                continue;

            // It is possible that there is only one underlying object, but you
            // need to go through several PHIs to see it, and thus could be
            // repeated in the Objects list.
            SmallPtrSet<const Value *, 4> ObjSet;
            SmallVector<Metadata *, 4> Scopes, NoAliases;

            SmallSetVector<const Argument *, 4> NAPtrArgs;
            for (unsigned i = 0, ie = PtrArgs.size(); i != ie; ++i) {
                SmallVector<Value *, 4> Objects;
                GetUnderlyingObjects(const_cast<Value*>(PtrArgs[i]), Objects,
                        DL, /* MaxLookup = */0);

                for (Value *O : Objects)
                    ObjSet.insert(O);
            }

            // Figure out if we're derived from anything that is not a noalias
            // argument.
            bool CanDeriveViaCapture = false, UsesAliasingPtr = false;
            for (const Value *V : ObjSet) {
                // Is this value a constant that cannot be derived from any pointer
                // value (we need to exclude constant expressions, for example, that
                // are formed from arithmetic on global symbols).
                bool IsNonPtrConst = isa<ConstantInt>(V) || isa<ConstantFP>(V)
                        || isa<ConstantPointerNull>(V)
                        || isa<ConstantDataVector>(V) || isa<UndefValue>(V);
                if (IsNonPtrConst)
                    continue;

                // If this is anything other than a noalias argument, then we cannot
                // completely describe the aliasing properties using alias.scope
                // metadata (and, thus, won't add any).
                if (const Argument *A = dyn_cast<Argument>(V)) {
                    if (!A->hasNoAliasAttr())
                        UsesAliasingPtr = true;
                }
                else {
                    UsesAliasingPtr = true;
                }

                // If this is not some identified function-local object (which cannot
                // directly alias a noalias argument), or some other argument (which,
                // by definition, also cannot alias a noalias argument), then we could
                // alias a noalias argument that has been captured).
                if (!isa<Argument>(V)
                        && !isIdentifiedFunctionLocal(const_cast<Value*>(V)))
                    CanDeriveViaCapture = true;
            }

            // A function call can always get captured noalias pointers (via other
            // parameters, globals, etc.).
            if (IsFuncCall && !IsArgMemOnlyCall)
                CanDeriveViaCapture = true;

            // First, we want to figure out all of the sets with which we definitely
            // don't alias. Iterate over all noalias set, and add those for which:
            //   1. The noalias argument is not in the set of objects from which we
            //      definitely derive.
            //   2. The noalias argument has not yet been captured.
            // An arbitrary function that might load pointers could see captured
            // noalias arguments via other noalias arguments or globals, and so we
            // must always check for prior capture.
            for (const Argument *A : FunArgs) {
                if (!ObjSet.count(A) && (!CanDeriveViaCapture ||
                // It might be tempting to skip the
                // PointerMayBeCapturedBefore check if
                // A->hasNoCaptureAttr() is true, but this is
                // incorrect because nocapture only guarantees
                // that no copies outlive the function, not
                // that the value cannot be locally captured.
                        !PointerMayBeCapturedBefore(A,
                        /* ReturnCaptures */false,
                        /* StoreCaptures */false, I, &DT)))
                    NoAliases.push_back(NewScopes[A]);
            }

            if (!NoAliases.empty())
                NI->setMetadata(LLVMContext::MD_noalias,
                        MDNode::concatenate(
                                NI->getMetadata(LLVMContext::MD_noalias),
                                MDNode::get(CalledFunc->getContext(),
                                        NoAliases)));

            // Next, we want to figure out all of the sets to which we might belong.
            // We might belong to a set if the noalias argument is in the set of
            // underlying objects. If there is some non-noalias argument in our list
            // of underlying objects, then we cannot add a scope because the fact
            // that some access does not alias with any set of our noalias arguments
            // cannot itself guarantee that it does not alias with this access
            // (because there is some pointer of unknown origin involved and the
            // other access might also depend on this pointer). We also cannot add
            // scopes to arbitrary functions unless we know they don't access any
            // non-parameter pointer-values.
            bool CanAddScopes = !UsesAliasingPtr;
            if (CanAddScopes && IsFuncCall)
                CanAddScopes = IsArgMemOnlyCall;

            if (CanAddScopes)
                for (const Argument *A : FunArgs) {
                    if (ObjSet.count(A))
                        Scopes.push_back(NewScopes[A]);
                }

            if (!Scopes.empty())
                NI->setMetadata(LLVMContext::MD_alias_scope,
                        MDNode::concatenate(
                                NI->getMetadata(LLVMContext::MD_alias_scope),
                                MDNode::get(CalledFunc->getContext(), Scopes)));
        }
    }
}

char AliasFunctionCloning::ID = 0;
static RegisterPass<AliasFunctionCloning> X("afc",
        "Alias Function Cloning pass", false, false);

