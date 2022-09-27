#pragma once

namespace arancini::output {
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
};
} // namespace arancini::output
