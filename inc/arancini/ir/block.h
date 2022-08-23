#pragma once

#include <arancini/ir/packet.h>
#include <arancini/ir/visitor.h>
#include <vector>

namespace arancini::ir {
class block {
public:
	packet *insert_packet()
	{
		auto p = new packet(*this);
		packets_.push_back(p);
		return p;
	}

	bool accept(visitor &v)
	{
		if (!v.visit_block(*this)) {
            return false;
        }

		for (auto p : packets_) {
			p->accept(v);
		}

        return true;
	}

private:
	std::vector<packet *> packets_;
};
} // namespace arancini::ir
