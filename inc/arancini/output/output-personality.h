#pragma once

#include <memory>

namespace arancini::output {
namespace mc {
	class machine_code_allocator;
}

using mc::machine_code_allocator;

enum class output_personality_kind { personality_static, personality_dynamic };

class output_personality {
public:
	virtual output_personality_kind kind() const = 0;
};

class static_output_personality : public output_personality {
public:
	virtual output_personality_kind kind() const override { return output_personality_kind::personality_static; }
};

class dynamic_output_personality : public output_personality {
public:
	virtual output_personality_kind kind() const override { return output_personality_kind::personality_dynamic; }

	virtual std::shared_ptr<machine_code_allocator> get_allocator() const = 0;
};
} // namespace arancini::output
