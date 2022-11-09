#include <arancini/ir/chunk.h>
#include <arancini/ir/default-ir-builder.h>
#include <arancini/ir/packet.h>

using namespace arancini::ir;

void default_ir_builder::begin_chunk()
{
	if (current_chunk_ != nullptr) {
		throw std::runtime_error("chunk already in progress");
	}

	current_chunk_ = std::make_shared<chunk>();
}

void default_ir_builder::end_chunk()
{
	if (current_pkt_ != nullptr) {
		throw std::runtime_error("packet in progress");
	}

	if (current_chunk_ == nullptr) {
		throw std::runtime_error("chunk not in progress");
	}

	if (chunk_complete_) {
		throw std::runtime_error("chunk already completed");
	}

	chunk_complete_ = true;
}

void default_ir_builder::begin_packet(off_t address)
{
	if (current_chunk_ == nullptr) {
		throw std::runtime_error("chunk not in progress");
	}

	if (chunk_complete_) {
		throw std::runtime_error("chunk is completed");
	}

	if (current_pkt_) {
		throw std::runtime_error("packet already in progress");
	}

	current_pkt_ = std::make_shared<packet>(address);
}

void default_ir_builder::end_packet()
{
	if (!current_pkt_) {
		throw std::runtime_error("packet not in progress");
	}

	current_chunk_->add_packet(current_pkt_);
	current_pkt_ = nullptr;
}

void default_ir_builder::insert_action(action_node *a)
{
	if (!current_pkt_) {
		throw std::runtime_error("packet not in progress");
	}

	current_pkt_->append_action(a);
}
