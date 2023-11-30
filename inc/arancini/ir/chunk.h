#pragma once

#include <arancini/ir/packet.h>
#include <arancini/ir/visitor.h>
#include <memory>
#include <vector>

namespace arancini::ir {
class packet;

/**
 * @brief Represents a chunk of instruction packets.  No control flow is
 * implied or represented by this data structure.
 */
class chunk {
public:
	/**
	 * @brief Adds an instruction packet to the end of this chunk.
	 *
	 * @param p The instruction packet to append to this chunk.
	 */
	void add_packet(std::shared_ptr<packet> p) { packets_.push_back(p); }

	const std::vector<std::shared_ptr<packet>> packets() const { return packets_; }

	off_t address(void) { return address_; }

	void accept(visitor &v) { v.visit_chunk(*this); }

private:
	off_t address_;
	std::vector<std::shared_ptr<packet>> packets_;
};
} // namespace arancini::ir
