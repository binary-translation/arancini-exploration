#pragma once

#include <memory>
#include <string>
#include <vector>

namespace arancini::ir {
class chunk;
}

namespace arancini::output::o_static {
class static_output_engine {
public:
	static_output_engine(const std::string &output_filename)
		: output_filename_(output_filename)
		, ep_(0)
	{
	}

	const std::string &output_filename() const { return output_filename_; }

	void add_chunk(std::shared_ptr<ir::chunk> c) { chunks_.push_back(c); }

	const std::vector<std::shared_ptr<ir::chunk>> &chunks() const { return chunks_; }

	void set_entrypoint(off_t ep) { ep_ = ep; }
	off_t get_entrypoint() const { return ep_; }

	virtual void generate() = 0;

private:
	std::string output_filename_;
	std::vector<std::shared_ptr<ir::chunk>> chunks_;
	off_t ep_;
};
} // namespace arancini::output::o_static
