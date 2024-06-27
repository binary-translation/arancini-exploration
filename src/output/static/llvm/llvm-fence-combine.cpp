#include "arancini/output/static/llvm/llvm-fence-combine.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include <llvm/Support/AtomicOrdering.h>

using namespace ::llvm;


PreservedAnalyses FenceCombinePass::run(Function &F, FunctionAnalysisManager &AM) {
	for (BasicBlock &BB : F) {

		auto It = BB.begin();
		IRBuilder<> builder(F.getContext());

		FenceInst *first = nullptr;
		while (It != BB.end()) {
			Instruction &I = *It;

			if (auto *Fence1 = dyn_cast<FenceInst>(&I)) {
				if (first) {
					It++;
					// combine the two fences
					first->setOrdering(getMergedAtomicOrdering(first->getOrdering(), Fence1->getOrdering()));
					Fence1->eraseFromParent();
					first = nullptr;
					continue;
				} else {
					first = Fence1;
				}
			} else {
				if (It->mayReadOrWriteMemory())
					first = nullptr;
			}
			It++;
		}
	}
	// probably true?
	return PreservedAnalyses::all();
}
