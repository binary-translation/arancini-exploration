#pragma once

#include <arancini/output/dynamic/translation-context.h>

namespace keystone {
#include <keystone/keystone.h>
}

namespace arancini::output::dynamic::arm64 {
class arm64_translation_context : public translation_context {
public:
	arm64_translation_context(machine_code_writer &writer)
		: translation_context(writer)
	{
        ks_err_; = keystone::ks_open(KS_ARCH_AARCH64, KS_MODE_64, &ks_);
        if (ks_err_ != KE_ERR_OK)
            throw std::runtime_error("AARCH64 DBT: ks_open() failed");
	}

	virtual void begin_block() override;
	virtual void begin_instruction(off_t address, const std::string &disasm) override;
	virtual void end_instruction() override;
	virtual void end_block() override;
	virtual void lower(ir::node *n) override;

    ~arm64_translation_context() {
        keystone::ks_close(ks_);
    }
private:
    keystone::ks_engine *ks_;
    keystone::ks_err ks_err_;

    void materialise(const ir::node *n);
};
} // namespace arancini::output::dynamic::arm64

