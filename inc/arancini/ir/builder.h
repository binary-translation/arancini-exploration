#pragma once

#include <arancini/ir/block.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/visitor.h>
#include <vector>

namespace arancini::ir {
class builder {
public:
	builder()
		: cur_(create_block())
	{
	}

	block *create_block()
	{
		auto b = new block();
		blocks_.push_back(b);

		return b;
	}

	packet *insert_packet() { return cur_->insert_packet(); }

	bool accept(visitor &v)
	{
		if (!v.visit_builder_start(*this)) {
            return false;
        }

		for (auto b : blocks_) {
			b->accept(v);
		}

        return v.visit_builder_end(*this);
	}

private:
	std::vector<block *> blocks_;
	block *cur_;
};
} // namespace arancini::ir
