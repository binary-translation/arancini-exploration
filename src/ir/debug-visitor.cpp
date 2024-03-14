#include <arancini/ir/chunk.h>
#include <arancini/ir/debug-visitor.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>
#include <arancini/input/x86/translators/translators.h>

#include <arancini/util/static-map.h>

using namespace arancini::ir;

void debug_visitor::visit_chunk(chunk &c)
{
	chunk_name_ = "c" + std::to_string(chunk_idx_++);
	packet_idx_ = 0;

	apply_indent();

	os_ << "chunk " << chunk_name_ << std::endl;
	indent();

	default_visitor::visit_chunk(c);

	outdent();
}

void debug_visitor::visit_packet(packet &p)
{
	packet_name_ = chunk_name_ + "p" + std::to_string(packet_idx_++);
	node_idx_ = 0;

	apply_indent();

	os_ << "packet " << std::hex << &p << std::endl;
	indent();

	default_visitor::visit_packet(p);

	outdent();
}

void debug_visitor::visit_node(node &n) { node_names_[&n] = packet_name_ + "n" + std::to_string(node_idx_++); }

void debug_visitor::visit_label_node(label_node &n)
{
	default_visitor::visit_label_node(n);

	apply_indent();
    os_ << fmt::format("{}: label\n", get_node_name(&n));
}

void debug_visitor::visit_cond_br_node(cond_br_node &n)
{
	default_visitor::visit_cond_br_node(n);

	apply_indent();
    os_ << fmt::format("{}: cond-br\n", get_node_name(&n));
}

void debug_visitor::visit_read_pc_node(read_pc_node &n)
{
	default_visitor::visit_read_pc_node(n);

	apply_indent();
    os_ << fmt::format("{}: read-pc\n", get_node_name(&n));
}

void debug_visitor::visit_write_pc_node(write_pc_node &n)
{
	default_visitor::visit_write_pc_node(n);

	apply_indent();
    os_ << fmt::format("{}: write-pc\n", get_node_name(&n));
}

void debug_visitor::visit_constant_node(constant_node &n)
{
	default_visitor::visit_constant_node(n);

	apply_indent();
    os_ << fmt::format("{}: const {:#x}\n", get_node_name(&n), n.const_val_i());
}

void debug_visitor::visit_read_reg_node(read_reg_node &n)
{
	default_visitor::visit_read_reg_node(n);

	apply_indent();
    os_ << fmt::format("{}: read-reg {}\n", get_node_name(&n), n.regname());
}

void debug_visitor::visit_read_mem_node(read_mem_node &n)
{
	default_visitor::visit_read_mem_node(n);

	apply_indent();
    os_ << fmt::format("{}: read-mem\n", get_node_name(&n));
}

void debug_visitor::visit_write_reg_node(write_reg_node &n)
{
	default_visitor::visit_write_reg_node(n);

	apply_indent();
    os_ << fmt::format("{}: write-reg {} {}\n", get_node_name(&n), n.regname(), get_port_name(n.value()));
}

void debug_visitor::visit_write_mem_node(write_mem_node &n)
{
	default_visitor::visit_write_mem_node(n);

	apply_indent();

    // TA: FIX
    os_ << fmt::format("{}: write-mem {} {}\n", get_node_name(&n), get_port_name(n.address()), get_port_name(n.value()));
}

void debug_visitor::visit_unary_arith_node(unary_arith_node &n)
{
	default_visitor::visit_unary_arith_node(n);

    // TA: refactor
	apply_indent();

    util::static_map<unary_arith_op, const char *, 3> matches {
        { unary_arith_op::bnot,       "not"        },   
        { unary_arith_op::neg,        "neg"        },   
        { unary_arith_op::complement, "complement" },   
    };

    auto match = matches.get(n.op(), "?");

    // TA: refactor
    os_ << fmt::format("{}: {} {}\n", get_node_name(&n), match, get_port_name(n.lhs()));
}

void debug_visitor::visit_binary_arith_node(binary_arith_node &n)
{
	default_visitor::visit_binary_arith_node(n);

    // TA: refactor
	apply_indent();

    util::static_map<binary_arith_op, const char *, 10> matches {
        { binary_arith_op::add,     "add"    },   
        { binary_arith_op::band,    "and"    },   
        { binary_arith_op::bor,     "or"     },   
        { binary_arith_op::bxor,    "xor"    },   
        { binary_arith_op::cmpeq,   "cmp-eq" },   
        { binary_arith_op::cmpne,   "cmp-ne" },   
        { binary_arith_op::cmpgt,   "cmp-gt" },   
        { binary_arith_op::div,     "div"    },   
        { binary_arith_op::mul,     "mul"    },   
        { binary_arith_op::sub,     "sub"    },   
        { binary_arith_op::mod,     "mod"    },   
    };

    auto match = matches.get(n.op(), "?");

    // TA: refactor
    os_ << fmt::format("{}: {} {}\n", get_node_name(&n), match, get_port_name(n.rhs()));
}

void debug_visitor::visit_ternary_arith_node(ternary_arith_node &n)
{
	default_visitor::visit_ternary_arith_node(n);

    // TA: refactor
	apply_indent();

    util::static_map<ternary_arith_op, const char *, 2> matches {
        { ternary_arith_op::adc, "adc" },   
        { ternary_arith_op::sbb, "sbb" },
    };

    auto match = matches.get(n.op(), "?");

    // TA: refactor
    os_ << fmt::format("{}: {} {}, {}\n", get_node_name(&n), match, get_port_name(n.lhs()), get_port_name(n.top()));
}

void debug_visitor::visit_unary_atomic_node(unary_atomic_node &n)
{
	default_visitor::visit_unary_atomic_node(n);

    // TA: refactor
	apply_indent();

    util::static_map<unary_atomic_op, const char *, 2> matches {
        { unary_atomic_op::neg,  "atomic neg" },   
        { unary_atomic_op::bnot, "atomic not" },
    };

    auto match = matches.get(n.op(), "?");

    // TA: refactor
    os_ << fmt::format("{}: {} {}\n", get_node_name(&n), match, get_port_name(n.lhs()));
}

void debug_visitor::visit_binary_atomic_node(binary_atomic_node &n)
{
	default_visitor::visit_binary_atomic_node(n);

    // TA: refactor
	apply_indent();

    util::static_map<binary_atomic_op, const char *, 9> matches {
        { binary_atomic_op::add,  "atomic add"   },   
        { binary_atomic_op::sub,  "atomic sub"   },
        { binary_atomic_op::band, "atomic and"   },
        { binary_atomic_op::bor,  "atomic or"    },
        { binary_atomic_op::xadd, "atomic xadd"  },
        { binary_atomic_op::bxor, "atomic xor"   },
        { binary_atomic_op::btc,  "atomic btc"   },
        { binary_atomic_op::btr,  "atomic btr"   },
        { binary_atomic_op::bts,  "atomic bts"   },
        { binary_atomic_op::xchg, "atomic xchg"  },
    };

    auto match = matches.get(n.op(), "?");

    // TA: refactor
    os_ << fmt::format("{}: {} {}, {}\n", get_node_name(&n), match, get_port_name(n.address()),
                                              get_port_name(n.rhs()));
}

void debug_visitor::visit_ternary_atomic_node(ternary_atomic_node &n)
{
	default_visitor::visit_ternary_atomic_node(n);
   
    // TA: refactor
	apply_indent();

    util::static_map<ternary_atomic_op, const char *, 3> matches {
        { ternary_atomic_op::adc,       "atomic adc"       },   
        { ternary_atomic_op::sbb,       "atomic sbb"       },
        { ternary_atomic_op::cmpxchg,   "atomic cmpxchg"   },
    };

    auto match = matches.get(n.op(), "?");

    // TA: refactor
    os_ << fmt::format("{}: {} {}, {}, {}\n", get_node_name(&n), match, get_port_name(n.address()),
                                              get_port_name(n.rhs()), get_port_name(n.top()));
}

void debug_visitor::visit_cast_node(cast_node &n) {
	default_visitor::visit_cast_node(n);

    // TA: refactor
	apply_indent();

    util::static_map<cast_op, const char *, 5> matches {
        { cast_op::bitcast, "bitcast"       },   
        { cast_op::convert, "convert"       },
        { cast_op::sx,      "sign-extend"   },
        { cast_op::trunc,   "truncate"      },
        { cast_op::zx,      "zero-extend"   }
    };

    auto match = matches.get(n.op(), "?");

    // TA: refactor
    os_ << fmt::format("{}: {} {} -> {}\n", get_node_name(&n), match, get_port_name(n.source_value()), n.val().type());
}

void debug_visitor::visit_csel_node(csel_node &n)
{
	default_visitor::visit_csel_node(n);

	apply_indent();
    os_ << fmt::format("{}: csel\n", get_node_name(&n));
}

void debug_visitor::visit_bit_shift_node(bit_shift_node &n)
{
	default_visitor::visit_bit_shift_node(n);

	apply_indent();
    os_ << fmt::format("{}: bit-shift\n", get_node_name(&n));
}

void debug_visitor::visit_bit_extract_node(bit_extract_node &n)
{
	default_visitor::visit_bit_extract_node(n);

	apply_indent();
    os_ << fmt::format("{}: bit-extract\n", get_node_name(&n));
}

void debug_visitor::visit_bit_insert_node(bit_insert_node &n)
{
	default_visitor::visit_bit_insert_node(n);

	apply_indent();
    os_ << fmt::format("{}: bit-insert\n", get_node_name(&n));
}

void debug_visitor::visit_vector_extract_node(vector_extract_node &n)
{
	default_visitor::visit_vector_extract_node(n);

	apply_indent();
    os_ << fmt::format("{}: vector-extract\n", get_node_name(&n));
}

void debug_visitor::visit_vector_insert_node(vector_insert_node &n)
{
	default_visitor::visit_vector_insert_node(n);

	apply_indent();
    os_ << fmt::format("{}: vector-insert\n", get_node_name(&n));
}
