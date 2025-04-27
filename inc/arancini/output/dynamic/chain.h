#pragma once

#include <arancini/output/dynamic/machine-code-allocator.h>
#include <arancini/output/dynamic/machine-code-writer.h>

namespace arancini::output::dynamic {
/**
 * Dummy allocator to allow overwriting code in chaining.
 * Only performs size checking.
 */
class chain_machine_code_allocator : public machine_code_allocator {
  public:
    explicit chain_machine_code_allocator(size_t size) : size_{size} {}
    void *allocate(void *original, size_t size) override {
        if (size <= size_) {
            return original;
        }
        return nullptr;
    }

  private:
    size_t size_;
};

/**
 * Writes code to a given address to allow overwriting code at that address in
 * chaining.
 */
class chain_machine_code_writer : public machine_code_writer {
  public:
    chain_machine_code_writer(void *target, size_t sizeE)
        : machine_code_writer(alloc_, (void *)((uintptr_t)target & ~0xfull),
                              ((uintptr_t)target & 0xfull),
                              sizeE + ((uintptr_t)target & 0xfull)),
          alloc_(sizeE + ((uintptr_t)target & 0xfull)) {}

    ~chain_machine_code_writer() { finalise(); }

  private:
    chain_machine_code_allocator alloc_;
};
} // namespace arancini::output::dynamic
