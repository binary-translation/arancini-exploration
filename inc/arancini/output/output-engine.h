#pragma once

#include <memory>
#include <vector>

namespace arancini::ir {
class chunk;
}

namespace arancini::output {
class output_personality;

class output_engine {
public:
	void add_chunk(std::shared_ptr<ir::chunk> c) { chunks_.push_back(c); }

	const std::vector<std::shared_ptr<ir::chunk>> &chunks() const { return chunks_; }

	virtual void generate(const output_personality &personality) = 0;

private:
	std::vector<std::shared_ptr<ir::chunk>> chunks_;
};
} // namespace arancini::output
