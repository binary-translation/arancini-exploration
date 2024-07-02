#include "llvm/IR/PassManager.h"

using namespace ::llvm;

class FenceCombinePass : public PassInfoMixin<FenceCombinePass> {
public:
	PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};
