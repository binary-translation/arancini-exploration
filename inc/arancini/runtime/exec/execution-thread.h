#pragma once

#include <bits/types/struct_timespec.h>
#include <ctime>
#include <iostream>
#include <memory>
#include <time.h>

#include <cstdint>

namespace arancini::runtime::exec {
class execution_context;

class execution_thread {
  public:
    execution_thread(execution_context &owner, size_t state_size);
    ~execution_thread();

    void *get_cpu_state() const { return cpu_state_; }

    void clk(char *s) {
        struct timespec ts;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
        std::cerr << std::dec << s << ":\t"
                  << ts.tv_sec * 1000000000 + ts.tv_nsec << std::endl;
    }

    uint64_t chain_address_;
    int *clear_child_tid_;

  private:
    execution_context &owner_;
    void *cpu_state_;
    size_t cpu_state_size_;
};
} // namespace arancini::runtime::exec
