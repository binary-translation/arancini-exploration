#include "arancini/ir/node.h"
#include "arancini/ir/port.h"
#include <arancini/output/static/llvm/llvm-static-output-engine-impl.h>
#include <arancini/output/static/llvm/llvm-static-output-engine.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <map>

using namespace arancini::output::o_static::llvm;
using namespace arancini::ir;
using namespace ::llvm;

Instruction *llvm_static_output_engine_impl::create_static_condbr(
        IRBuilder<> *builder, std::shared_ptr<packet> pkt, std::map<unsigned long, BasicBlock *> *blocks,
        BasicBlock *mid) {
    auto it = builder->GetInsertPoint();
    if ((--it)->getOpcode() != Instruction::Store)
        return nullptr;

    if ((--it)->getOpcode() != Instruction::Select)
        return nullptr;

    auto cond = it->getOperand(0);
    auto true_addr = dyn_cast<ConstantInt>(it->getOperand(1));
    auto false_addr = dyn_cast<ConstantInt>(it->getOperand(2));

    BasicBlock *true_block = mid;
    BasicBlock *false_block = mid;
    std::map<unsigned long, BasicBlock *>::iterator bb_it;

    // Add can constant fold on creation
    if (true_addr) {
        bb_it = blocks->find(true_addr->getZExtValue());
        true_block = bb_it != blocks->end() ? bb_it->second : mid;
    }

    if (false_addr) {
        bb_it = blocks->find(false_addr->getZExtValue());
        false_block = bb_it != blocks->end() ? bb_it->second : mid;
    }

    if (true_block != mid && false_block != mid)
        fixed_branches++;
    return builder->CreateCondBr(cond, true_block, false_block);
}
