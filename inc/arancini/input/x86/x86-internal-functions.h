#pragma once

#include <arancini/ir/internal-function-resolver.h>

namespace arancini::input::x86 {
class x86_internal_functions : public ir::internal_function_resolver {
protected:
	virtual std::shared_ptr<ir::internal_function> create(std::string_view name) const override;
};
} // namespace arancini::input::x86
