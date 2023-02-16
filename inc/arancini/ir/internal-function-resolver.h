#pragma once

#include <arancini/ir/node.h>
#include <map>

namespace arancini::ir {
class internal_function_resolver {
public:
	internal_function &resolve(const std::string &name)
	{
		auto intf = functions_.find(name);
		if (intf == functions_.end()) {
			auto newfn = create(name);
			if (!newfn) {
				throw std::runtime_error("unable to resolve internal function " + name);
			}
			functions_[name] = newfn;
		}

		return *functions_.at(name);
	}

protected:
	virtual internal_function *create(const std::string &name) const = 0;

private:
	std::map<std::string, internal_function *> functions_;
};
} // namespace arancini::ir
