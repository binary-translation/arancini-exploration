#pragma once

#include <arancini/util/static-map.h>
#include <arancini/ir/default-visitor.h>

#include <set>

namespace arancini::ir {
class dot_graph_generator : public default_visitor {
public:
	dot_graph_generator(FILE *out)
		: out_(out)
		, current_packet_(nullptr)
		, last_action_(nullptr)
		, cur_node_(nullptr)
	{
	}

	virtual void visit_chunk(chunk &c) override;
	virtual void visit_packet(packet &p) override;
	virtual void visit_node(node &n) override;
	virtual void visit_action_node(action_node &n) override;
	virtual void visit_label_node(label_node &n) override;
	virtual void visit_br_node(br_node &n) override;
	virtual void visit_cond_br_node(cond_br_node &n) override;
	virtual void visit_read_pc_node(read_pc_node &n) override;
	virtual void visit_write_pc_node(write_pc_node &n) override;
	virtual void visit_constant_node(constant_node &n) override;
	virtual void visit_read_reg_node(read_reg_node &n) override;
	virtual void visit_read_mem_node(read_mem_node &n) override;
	virtual void visit_write_reg_node(write_reg_node &n) override;
	virtual void visit_write_mem_node(write_mem_node &n) override;
	virtual void visit_unary_arith_node(unary_arith_node &n) override;
	virtual void visit_binary_arith_node(binary_arith_node &n) override;
	virtual void visit_ternary_arith_node(ternary_arith_node &n) override;
	virtual void visit_unary_atomic_node(unary_atomic_node &n) override;
	virtual void visit_binary_atomic_node(binary_atomic_node &n) override;
	virtual void visit_ternary_atomic_node(ternary_atomic_node &n) override;
	virtual void visit_cast_node(cast_node &n) override;
	virtual void visit_csel_node(csel_node &n) override;
	virtual void visit_bit_shift_node(bit_shift_node &n) override;
	virtual void visit_bit_extract_node(bit_extract_node &n) override;
	virtual void visit_bit_insert_node(bit_insert_node &n) override;
	virtual void visit_vector_extract_node(vector_extract_node &n) override;
	virtual void visit_vector_insert_node(vector_insert_node &n) override;
	virtual void visit_read_local_node(read_local_node &n) override;
	virtual void visit_write_local_node(write_local_node &n) override;
	virtual void visit_internal_call_node(internal_call_node &n) override;

private:
	FILE *out_;
	packet *current_packet_;
	action_node *last_action_;
	node *cur_node_;
	std::set<node *> seen_;

	void add_node(const node *n, const std::string &label) { 
        fmt::print(out_, "N{} [shape=Mrecord, label=\"{}\"]\n", fmt::ptr(n), label);
    }

	std::string compute_port_label(const port *p) const
	{
        // TA: FIXME
        util::static_map<port_kinds, std::string, 5> matches = {
            { port_kinds::value, "value"},
            { port_kinds::constant, "#"},
            { port_kinds::negative, "N"},
            { port_kinds::overflow, "V"},
            { port_kinds::carry, "C"},
            { port_kinds::zero, "Z"}
        };

        auto match = matches.get(p->kind(), "?");
		return fmt::format("{}:{}", match, p->type());
	}

	void add_port_edge(const port *from, const node *to, const std::string &link = "") { add_edge(from->owner(), to, "black", compute_port_label(from), link); }

	void add_control_edge(const node *from, const node *to, const std::string &link = "") { add_edge(from, to, "green3", "", link); }

	void add_edge(const node *from, const node *to, const std::string &colour = "black", const std::string &label = "", const std::string &link = "")
	{
        std::string str;
        fmt::format_to(std::back_inserter(str), "N{} -> N{}", fmt::ptr(from), fmt::ptr(to));

		if (link != "") {
            fmt::format_to(std::back_inserter(str), ":{}", link);
		}

        fmt::format_to(std::back_inserter(str), " [color={}", colour);

		if (label != "") {
            fmt::format_to(std::back_inserter(str), ", label=\"{}\"", label);
		}

        fmt::print(out_, "{}]\n", str);
	}
};
} // namespace arancini::ir
