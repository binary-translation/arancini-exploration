#pragma once

#include <arancini/ir/node.h>
#include <map>

namespace arancini::ir {
class internal_function_resolver {
public:
	const std::shared_ptr<internal_function> &resolve(const std::string &name)
	{
		auto intf = functions_.find(name);
		if (intf == functions_.end()) {
			auto newfn = create(name);
			if (!newfn) {
				throw std::runtime_error("unable to resolve internal function " + name);
			}
			functions_[name] = std::move(newfn);
		}

		return functions_.at(name);
	}

protected:
	virtual std::shared_ptr<internal_function> create(const std::string &name) const = 0;

private:
	std::map<std::string, std::shared_ptr<internal_function>> functions_;
};
} // namespace arancini::ir
