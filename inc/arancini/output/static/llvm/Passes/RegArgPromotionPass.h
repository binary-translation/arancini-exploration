#pragma once

#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/NoFolder.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Analysis/LazyCallGraph.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <unordered_set>
#include <unordered_map>


namespace arancini::output::o_static::llvm {
    using namespace ::llvm;

    class RegArgPromotionPass : public PassInfoMixin<RegArgPromotionPass> {
    public:
        RegArgPromotionPass(llvm::Type *cpuStateType) : cpuStateType(cpuStateType) {}

        PreservedAnalyses
        run(llvm::LazyCallGraph::SCC &C, AnalysisManager<llvm::LazyCallGraph::SCC, llvm::LazyCallGraph &> &AM,
            llvm::LazyCallGraph &CG, llvm::CGSCCUpdateResult &UR);

    private:
        llvm::Type *cpuStateType;

        Function *promoteRegToArg(Function *F);

        // A map from function to its promoted registers
        std::unordered_map<Function *, std::unordered_set<size_t>> promotedRegs;
    };
}
