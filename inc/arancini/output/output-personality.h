#pragma once

#include <memory>
#include <string>

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
	static_output_personality(const std::string &output_file)
		: output_file_(output_file)
	{
	}

	virtual output_personality_kind kind() const override { return output_personality_kind::personality_static; }

	const std::string &output_file() const { return output_file_; }

private:
	std::string output_file_;
};

class dynamic_output_personality : public output_personality {
public:
	dynamic_output_personality()
		: ep_(nullptr)
	{
	}

	virtual output_personality_kind kind() const override { return output_personality_kind::personality_dynamic; }

	virtual std::shared_ptr<machine_code_allocator> get_allocator() const = 0;

	void entrypoint(const void *p) { ep_ = p; }
	const void *entrypoint() const { return ep_; }

private:
	const void *ep_;
};
} // namespace arancini::output
