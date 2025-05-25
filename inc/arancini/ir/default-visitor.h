#pragma once

#include <arancini/ir/chunk.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>
#include <arancini/ir/visitor.h>
#include <set>

namespace arancini::ir {
class default_visitor : public visitor {
  public:
    virtual void visit_chunk(chunk &c) override {
        for (const auto &p : c.packets()) {
            p->accept(*this);
        }
    }

    virtual void visit_packet(packet &p) override {
        for (auto n : p.actions()) {
            n->accept(*this);
        }
    }

    // Nodes
    virtual void visit_node(node &) override {}

    virtual void visit_action_node(action_node &) override {}

    virtual void visit_label_node(label_node &) override {}

    virtual void visit_value_node(value_node &) override {}

    virtual void visit_br_node(br_node &) override {}

    virtual void visit_cond_br_node(cond_br_node &n) override {
        n.cond().accept(*this);
    }

    virtual void visit_read_pc_node(read_pc_node &) override {}

    virtual void visit_write_pc_node(write_pc_node &n) override {
        n.value().accept(*this);
    }

    virtual void visit_constant_node(constant_node &) override {}

    virtual void visit_read_reg_node(read_reg_node &) override {}

    virtual void visit_read_mem_node(read_mem_node &n) override {
        n.address().accept(*this);
    }

    virtual void visit_write_reg_node(write_reg_node &n) override {
        n.value().accept(*this);
    }

    virtual void visit_write_mem_node(write_mem_node &n) override {
        n.address().accept(*this);
        n.value().accept(*this);
    }

    virtual void visit_arith_node(arith_node &) override {}

    virtual void visit_unary_arith_node(unary_arith_node &n) override {
        n.lhs().accept(*this);
    }

    virtual void visit_binary_arith_node(binary_arith_node &n) override {
        n.lhs().accept(*this);
        n.rhs().accept(*this);
    }

    virtual void visit_ternary_arith_node(ternary_arith_node &n) override {
        n.lhs().accept(*this);
        n.rhs().accept(*this);
        n.top().accept(*this);
    }

    virtual void visit_atomic_node(atomic_node &) override {}

    virtual void visit_unary_atomic_node(unary_atomic_node &n) override {
        n.lhs().accept(*this);
    }

    virtual void visit_binary_atomic_node(binary_atomic_node &n) override {
        n.address().accept(*this);
        n.rhs().accept(*this);
    }

    virtual void visit_ternary_atomic_node(ternary_atomic_node &n) override {
        n.address().accept(*this);
        n.rhs().accept(*this);
        n.top().accept(*this);
    }

    virtual void visit_cast_node(cast_node &n) override {
        n.source_value().accept(*this);
    }

    virtual void visit_csel_node(csel_node &n) override {
        n.condition().accept(*this);
        n.trueval().accept(*this);
        n.falseval().accept(*this);
    }

    virtual void visit_bit_shift_node(bit_shift_node &n) override {
        n.input().accept(*this);
        n.amount().accept(*this);
    }

    virtual void visit_bit_extract_node(bit_extract_node &n) override {
        n.source_value().accept(*this);
    }

    virtual void visit_bit_insert_node(bit_insert_node &n) override {
        n.source_value().accept(*this);
        n.bits().accept(*this);
    }

    virtual void visit_vector_node(vector_node &) override {}

    virtual void visit_vector_element_node(vector_element_node &) override {}

    virtual void visit_vector_extract_node(vector_extract_node &n) override {
        n.source_vector().accept(*this);
    }

    virtual void visit_vector_insert_node(vector_insert_node &n) override {
        n.source_vector().accept(*this);
        n.insert_value().accept(*this);
    }

    virtual void visit_read_local_node(read_local_node &) override {}
    virtual void visit_write_local_node(write_local_node &n) override {
        n.write_value().accept(*this);
    }

    virtual void visit_internal_call_node(internal_call_node &n) override {
        for (auto *p : n.args()) {
            p->accept(*this);
        }
    }

    virtual void visit_port(port &p) override {
        if (seen_.count(p.owner())) {
            return;
        }

        seen_.insert(p.owner());
        p.owner()->accept(*this);
    }

    virtual bool seen_node(node *n) override { return seen_.count(n) > 0; }

    virtual void reset() override { seen_.clear(); }

  private:
    std::set<node *> seen_;
};
} // namespace arancini::ir
