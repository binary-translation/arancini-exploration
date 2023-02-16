#include <arancini/ir/chunk.h>
#include <arancini/ir/default-ir-builder.h>
#include <arancini/ir/internal-function-resolver.h>
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

void default_ir_builder::begin_packet(off_t address, const std::string &disassembly)
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

	current_pkt_ = std::make_shared<packet>(address, disassembly);
}

packet_type default_ir_builder::end_packet()
{
	if (!current_pkt_) {
		throw std::runtime_error("packet not in progress");
	}

	bool eob = current_pkt_->updates_pc();

	current_chunk_->add_packet(current_pkt_);
	current_pkt_ = nullptr;

	return eob ? packet_type::end_of_block : packet_type::normal;
}

const local_var &default_ir_builder::alloc_local(const value_type &type)
{
	if (current_pkt_ == nullptr) {
		throw std::runtime_error("cannot allocate local variable without packet");
	}

	return current_pkt_->alloc_local(type);
}

void default_ir_builder::insert_action(action_node *a)
{
	if (!current_pkt_) {
		throw std::runtime_error("packet not in progress");
	}

	current_pkt_->append_action(a);
}

void default_ir_builder::process_node(node *n)
{
	if (debug_ && current_pkt_) {
		n->set_metadata("guest-address", std::make_shared<numeric_value_metadata>(current_pkt_->address()));
	}
}
