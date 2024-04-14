#include "arancini/output/static/llvm/llvm-fence-combine.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"

using namespace ::llvm;


PreservedAnalyses FenceCombinePass::run(Function &F, FunctionAnalysisManager &AM) {
	for (BasicBlock &BB : F) {

		auto It = BB.begin();
		
		while (It != BB.end()) {
			Instruction &I = *It;
			It++;

			// Combines two consecutive fence instructions into a single one.
			if (auto *Fence1 = dyn_cast<FenceInst>(&I)) {
				if (It == BB.end())
					break;
				Instruction &NextI = *It;
				if (auto *Fence2 = dyn_cast<FenceInst>(&NextI)) {
					It++;
					auto ord1 = Fence1->getOrdering();
					auto ord2 = Fence2->getOrdering();
					// combine orderings, ignore consume because no one implements it
					// most common case is aquire and release to aquire-release
					if (ord1 == AtomicOrdering::Acquire && ord2 == AtomicOrdering::Release) {
						Fence1->setOrdering(AtomicOrdering::AcquireRelease);
					}
					else if (ord1 == AtomicOrdering::Release && ord2 == AtomicOrdering::Acquire) {
						Fence1->setOrdering(AtomicOrdering::AcquireRelease);
					}
					else if (ord1 == AtomicOrdering::SequentiallyConsistent || ord2 == AtomicOrdering::SequentiallyConsistent) {
						Fence1->setOrdering(AtomicOrdering::SequentiallyConsistent);
					}
					else if (ord1 == AtomicOrdering::AcquireRelease || ord2 == AtomicOrdering::AcquireRelease) {
						Fence1->setOrdering(AtomicOrdering::AcquireRelease);
					}

					// assume the scopes are the same
					Fence2->eraseFromParent();
				}
			}
		}
	}
	// probably true?
	return PreservedAnalyses::all();
}

