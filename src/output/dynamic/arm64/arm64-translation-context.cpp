#include <arancini/output/dynamic/arm64/arm64-translation-context.h>

using namespace arancini::output::dynamic::arm64;
using namespace arancini::ir;

void arm64_translation_context::begin_block() { }

void arm64_translation_context::begin_instruction(off_t address, const std::string &disasm) {
	instruction_index_to_guest_[builder_.nr_instructions()] = address;

	this_pc_ = address;
	std::cerr << "  " << std::hex << address << ": " << disasm << std::endl;
}

void arm64_translation_context::end_instruction() { }

void arm64_translation_context::end_block() {
	do_register_allocation();

    // TODO: add operations to finish block
	// builder_.xor_(
	// 	x86_operand(x86_physical_register_operand(x86_register_names::AX), 32), x86_operand(x86_physical_register_operand(x86_register_names::AX), 32));
	// builder_.ret();

	builder_.dump(std::cerr);

	builder_.emit(writer());

    // TODO: add code to dump
}

void arm64_translation_context::lower(ir::node *n) {
    materialise(n);
}

static arm64_operand virtreg_operand(unsigned int index, int width) {
    return arm64_operand(arm64_vreg_op(index, width == 1 ? 8 : width));
}

static arm64_operand imm_operand(unsigned long value, int width) {
    return arm64_operand(arm64_immediate_operand(value, width == 1 ? 8 : width));
}

arm64_operand
arm64_translation_context::guestreg_memory_operand(int width, int regoff,
                                                   bool pre, bool post)
{
    // FIXME: handle width
    arm64_memory_operand mem;
    if (regoff > 255 || regoff < -256) {
        auto base_vreg = alloc_vreg();
        auto preg = arm64_physreg_op(arm64_physreg_op::x29);
        builder_.mov(virtreg_operand(base_vreg, width),
                     imm_operand(regoff, 16));
        builder_.add(virtreg_operand(base_vreg, width),
                     arm64_operand(preg),
                     virtreg_operand(base_vreg, width));
        mem = arm64_memory_operand(arm64_vreg_op(base_vreg, width), 0, pre, post);
    } else {
        auto preg = arm64_physreg_op(arm64_physreg_op::xzr_sp);
        mem = arm64_memory_operand(preg, regoff, pre, post);
    }

	return arm64_operand(mem);
}

arm64_operand arm64_translation_context::vreg_operand_for_port(port &p, bool constant_fold) {
    // TODO
	if (constant_fold) {
		if (p.owner()->kind() == node_kinds::read_pc) {
			return arm64_operand(arm64_immediate_operand(this_pc_, 64));
		} else if (p.owner()->kind() == node_kinds::constant) {
			return arm64_operand(arm64_immediate_operand(((constant_node *)p.owner())->const_val_i(), p.type().width()));
		}
	}

	materialise(p.owner());
	return virtreg_operand(vreg_for_port(p), p.type().element_width());
}

static void func_push_args(arm64_instruction_builder *builder, const
                           std::vector<port *> &args)
{
    if (!builder)
        throw std::runtime_error("Instruction builder is invalid");

    for (auto* arg : args) {
        // TODO: handle the calling convention
        // this depends on the expectations of the function
        // TODO: maybe mark all functions as asm_linkage for stack-based
        // argument passing?
    }
}

void arm64_translation_context::materialise(const ir::node* n) {
    if (!n)
        throw std::runtime_error("ARM64 DBT received NULL pointer to node");

    switch (n->kind()) {
    case node_kinds::read_reg:
        return materialise_read_reg(*reinterpret_cast<const read_reg_node*>(n));
    case node_kinds::write_reg:
        return materialise_write_reg(*reinterpret_cast<const write_reg_node*>(n));
    case node_kinds::read_mem:
        return materialise_read_mem(*reinterpret_cast<const read_mem_node*>(n));
    case node_kinds::write_mem:
        return materialise_write_mem(*reinterpret_cast<const write_mem_node*>(n));
	case node_kinds::read_pc:
		return materialise_read_pc(*reinterpret_cast<const read_pc_node *>(n));
	case node_kinds::write_pc:
		return materialise_write_pc(*reinterpret_cast<const write_pc_node *>(n));
    case node_kinds::label:
        return materialise_label(*reinterpret_cast<const label_node *>(n));
    case node_kinds::br:
        return materialise_br(*reinterpret_cast<const br_node *>(n));
    case node_kinds::cond_br:
        return materialise_cond_br(*reinterpret_cast<const cond_br_node *>(n));
	case node_kinds::cast:
		return materialise_cast(*reinterpret_cast<const cast_node *>(n));
    case node_kinds::csel:
		return materialise_csel(*reinterpret_cast<const csel_node *>(n));
    case node_kinds::bit_shift:
		return materialise_bit_shift(*reinterpret_cast<const bit_shift_node *>(n));
    case node_kinds::bit_extract:
		return materialise_bit_extract(*reinterpret_cast<const bit_extract_node *>(n));
    case node_kinds::bit_insert:
		return materialise_bit_insert(*reinterpret_cast<const bit_insert_node *>(n));
    case node_kinds::constant:
        return materialise_constant(*reinterpret_cast<const constant_node*>(n));
	case node_kinds::unary_arith:
        return materialise_unary_arith(*reinterpret_cast<const unary_arith_node*>(n));
	case node_kinds::binary_arith:
		return materialise_binary_arith(*reinterpret_cast<const binary_arith_node*>(n));
    case node_kinds::internal_call:
        return materialise_internal_call(*reinterpret_cast<const internal_call_node*>(n));
    default:
        throw std::runtime_error("unknown node encountered: " +
                                 std::to_string(static_cast<size_t>(n->kind())));
    }
}

void arm64_translation_context::materialise_read_reg(const read_reg_node &n) {
	int value_vreg = alloc_vreg_for_port(n.val());
	int w = n.val().type().element_width() == 1 ? 8 : n.val().type().element_width();

	builder_.ldr(virtreg_operand(value_vreg, w),
                 guestreg_memory_operand(w, n.regoff()));
}

void arm64_translation_context::materialise_write_reg(const write_reg_node &n) {
	int w = n.val().type().element_width() == 1 ? 8 : n.val().type().element_width();

	// Detect register decrement/increment : write X <= Read X (+/-) Constant
	if (n.value().owner()->kind() == node_kinds::binary_arith) {
		auto binop = (binary_arith_node *)n.value().owner();

		if (binop->lhs().owner()->kind() == node_kinds::read_reg &&
                binop->rhs().owner()->kind() == node_kinds::constant)
        {
			auto lhs = (read_reg_node *)binop->lhs().owner();
			auto rhs = (constant_node *)binop->rhs().owner();


            if (lhs->regoff() == n.regoff()) {
               // TODO
                switch (binop->op()) {
                case binary_arith_op::sub:
                    builder_.sub(guestreg_memory_operand(w, n.regoff()),
                        arm64_operand(arm64_immediate_operand(rhs->const_val_i(), w)));
                    return;
                   default:
                       throw std::runtime_error("unexpected");
                }
            }
		}
	}

	if (n.value().owner()->kind() == node_kinds::constant) {
        // FIXME
		auto cv = (constant_node *)n.value().owner();
		builder_.mov(guestreg_memory_operand(w, n.regoff()),
                     imm_operand(cv->const_val_i(), w));
	} else {
		builder_.str(vreg_operand_for_port(n.value()),
                     guestreg_memory_operand(w, n.regoff()));
	}
}

void arm64_translation_context::materialise_read_mem(const read_mem_node &n) {
	int value_vreg = alloc_vreg_for_port(n.val());
	int w = n.val().type().element_width();

	materialise(n.address().owner());
	int addr_vreg = vreg_for_port(n.address());

    // TODO
    auto mem = arm64_memory_operand(arm64_vreg_op(addr_vreg, w));
	builder_.ldr(virtreg_operand(value_vreg, w),
                 arm64_operand(mem));
}

void arm64_translation_context::materialise_write_mem(const write_mem_node &n) {
	int w = n.value().type().element_width();

	materialise(n.address().owner());
	int addr_vreg = vreg_for_port(n.address());

    auto mem = arm64_memory_operand(arm64_vreg_op(addr_vreg, w), 0, false, true);
	builder_.str(vreg_operand_for_port(n.value()),
                 arm64_operand(mem));
}

void arm64_translation_context::materialise_read_pc(const read_pc_node &n)
{
	int dst_vreg = alloc_vreg_for_port(n.val());

	builder_.mov(virtreg_operand(dst_vreg, n.val().type().element_width()),
                 arm64_operand(arm64_immediate_operand(this_pc_, 64)));
}

void arm64_translation_context::materialise_write_pc(const write_pc_node &n)
{
	int dst_vreg = alloc_vreg_for_port(n.val());

    // TODO
}

void arm64_translation_context::materialise_label(const label_node &n) {
    builder_.label(n.name(), instr_cnt_);
}

void arm64_translation_context::materialise_br(const br_node &n) {
    materialise(n.target());

    builder_.b(n.target()->name());
}

void arm64_translation_context::materialise_cond_br(const cond_br_node &n) {
    materialise(n.target());

	int cond_vreg = alloc_vreg_for_port(n.cond());

    // builder_.cmp(cond_vreg, imm_operand(0, 64));
    builder_.beq(n.target()->name());
}

void arm64_translation_context::materialise_constant(const constant_node &n) {
	int dst_vreg = alloc_vreg_for_port(n.val());
	builder_.mov(virtreg_operand(dst_vreg, n.val().type().element_width()), imm_operand(n.const_val_i(), n.val().type().element_width()));
}

void arm64_translation_context::materialise_binary_arith(const binary_arith_node &n) {
	int val_vreg = alloc_vreg_for_port(n.val());
	int z_vreg = alloc_vreg_for_port(n.zero());
	int n_vreg = alloc_vreg_for_port(n.negative());
	int v_vreg = alloc_vreg_for_port(n.overflow());
	int c_vreg = alloc_vreg_for_port(n.carry());

	int w = n.val().type().element_width();

	switch (n.op()) {
	case binary_arith_op::add:
		builder_.add(virtreg_operand(val_vreg, w),
                     vreg_operand_for_port(n.lhs()),
                     vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::sub:
		builder_.sub(virtreg_operand(val_vreg, w),
                     vreg_operand_for_port(n.lhs()),
                     vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::mul:
		builder_.mul(virtreg_operand(val_vreg, w),
                     vreg_operand_for_port(n.lhs()),
                     vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::div:
		builder_.sdiv(virtreg_operand(val_vreg, w),
                      vreg_operand_for_port(n.lhs()),
                      vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::mod:
		builder_.and_(virtreg_operand(val_vreg, w),
                      vreg_operand_for_port(n.lhs()),
                      vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::bor:
		builder_.or_(virtreg_operand(val_vreg, w), vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::band:
		builder_.and_(virtreg_operand(val_vreg, w), vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::bxor:
		builder_.xor_(virtreg_operand(val_vreg, w), vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::cmpeq:
        // NOTE: set*() will modify the flags
        builder_.cmp(vreg_operand_for_port(n.lhs()), vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::cmpne:
        builder_.cmp(vreg_operand_for_port(n.lhs()), vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::cmpgt:
        builder_.cmp(vreg_operand_for_port(n.lhs()), vreg_operand_for_port(n.rhs()));
		break;
	default:
		throw std::runtime_error("unsupported binary arithmetic operation " + std::to_string((int)n.op()));
	}

	builder_.setz(virtreg_operand(z_vreg, 8));
	builder_.seto(virtreg_operand(v_vreg, 8));
	builder_.setc(virtreg_operand(c_vreg, 8));
	builder_.sets(virtreg_operand(n_vreg, 8));
}

void arm64_translation_context::materialise_unary_arith(const unary_arith_node &n) {
	int val_vreg = alloc_vreg_for_port(n.val());

    int w = n.val().type().element_width();

    materialise(n.lhs().owner());

    switch (n.op()) {
    case unary_arith_op::bnot:
        builder_.not_(virtreg_operand(val_vreg, w), vreg_operand_for_port(n.lhs()));
        break;
    case unary_arith_op::neg:
        // neg: ~reg + 1 for complement-of-2
        builder_.not_(virtreg_operand(val_vreg, w), vreg_operand_for_port(n.lhs()));
        builder_.add(virtreg_operand(val_vreg, w),
                     imm_operand(1, 64));
        break;
    default:
        throw std::runtime_error("Unknown unary operation");
    }
}

void arm64_translation_context::materialise_cast(const cast_node &n) {
	int dst_vreg = alloc_vreg_for_port(n.val());

	switch (n.op()) {
	case cast_op::zx:
		builder_.movz(virtreg_operand(dst_vreg, n.val().type().element_width()),
                      vreg_operand_for_port(n.source_value()), imm_operand(0, 64));
		break;

	case cast_op::sx:
		builder_.movn(virtreg_operand(dst_vreg, n.val().type().element_width()),
                      vreg_operand_for_port(n.source_value(), false), imm_operand(0, 64));
		break;

	case cast_op::bitcast:
		builder_.mov(virtreg_operand(dst_vreg, n.val().type().element_width()),
                     vreg_operand_for_port(n.source_value()));
		break;

	default:
		throw std::runtime_error("unsupported cast operation");
	}
}

void arm64_translation_context::materialise_csel(const csel_node &n) {
    int dst_vreg = alloc_vreg_for_port(n.val());

    builder_.csel(virtreg_operand(dst_vreg, n.val().type().element_width()),
                  vreg_operand_for_port(n.trueval(), false),
                  vreg_operand_for_port(n.falseval(), false),
                  vreg_operand_for_port(n.condition(), false));
}

void arm64_translation_context::materialise_bit_shift(const bit_shift_node &n) {
    int dst_vreg = alloc_vreg_for_port(n.val());

    switch (n.op()) {
    case shift_op::lsl:
        builder_.lsl(virtreg_operand(dst_vreg, n.val().type().element_width()),
                     vreg_operand_for_port(n.input(), false),
                     vreg_operand_for_port(n.amount(), false));
        break;
    case shift_op::lsr:
        builder_.lsr(virtreg_operand(dst_vreg, n.val().type().element_width()),
                     vreg_operand_for_port(n.input(), false),
                     vreg_operand_for_port(n.amount(), false));
        break;
    case shift_op::asr:
        builder_.asr(virtreg_operand(dst_vreg, n.val().type().element_width()),
                     vreg_operand_for_port(n.input(), false),
                     vreg_operand_for_port(n.amount(), false));
        break;
    default:
        throw std::runtime_error("unsupported shift operation: " +
                                 std::to_string(static_cast<int>(n.op())));
    }
}

void arm64_translation_context::materialise_bit_extract(const bit_extract_node &n) {
    int dst_vreg = alloc_vreg_for_port(n.val());

    builder_.ubfx(virtreg_operand(dst_vreg, n.val().type().element_width()),
                  vreg_operand_for_port(n.source_value(), false),
                  imm_operand(n.from(), 64),
                  imm_operand(n.length(), 64));
}

void arm64_translation_context::materialise_bit_insert(const bit_insert_node &n) {
    int dst_vreg = alloc_vreg_for_port(n.val());

    // TODO
    builder_.bfi(virtreg_operand(dst_vreg, n.val().type().element_width()),
                 vreg_operand_for_port(n.bits(), false),
                 imm_operand(n.to(), 64),
                 imm_operand(n.length(), 64));
}

void arm64_translation_context::materialise_internal_call(const internal_call_node &n) {
    // TODO
    func_push_args(&builder_, n.args());
    builder_.bl(n.fn().name());
}

void arm64_translation_context::do_register_allocation() { builder_.allocate(); }

