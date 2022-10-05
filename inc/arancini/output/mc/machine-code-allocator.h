#pragma once

#include <cstdlib>

namespace arancini::output::mc {
class machine_code_allocator {
public:
	virtual void *alloc(size_t size) = 0;
	virtual void *realloc(void *p, size_t size) = 0;
	virtual void free(void *p) = 0;
};

class default_machine_code_allocator : public machine_code_allocator {
public:
	virtual void *alloc(size_t size) override { return std::malloc(size); }
	virtual void *realloc(void *p, size_t size) override { return std::realloc(p, size); }
	virtual void free(void *p) override { std::free(p); }
};
} // namespace arancini::output::mc
