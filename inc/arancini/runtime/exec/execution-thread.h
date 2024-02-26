#pragma once

#include <bits/types/struct_timespec.h>
#include <ctime>
#include <memory>
#include <time.h>
#include <iostream>

namespace arancini::runtime::exec {
class execution_context;

class execution_thread {
public:
	execution_thread(execution_context &owner, size_t state_size);
	~execution_thread();

	void *get_cpu_state() const { return cpu_state_; }
	std::pair<struct timespec, struct timespec> get_ts() { return {entry_, exit_}; };

	void clk_entry() {
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &entry_);
		guest_time += ((exit_.tv_sec+exit_.tv_nsec/1000000000.0) - (entry_.tv_sec+entry_.tv_nsec/1000000000.0));
	}
	void clk_exit() {
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &exit_);
		run_time += ((entry_.tv_sec+entry_.tv_nsec/1000000000.0) - (exit_.tv_sec+exit_.tv_nsec/1000000000.0));
	}

	uint64_t chain_address_;
	int* clear_child_tid_;
private:
	execution_context &owner_;
	void *cpu_state_;
	size_t cpu_state_size_;
	struct timespec entry_;
	struct timespec exit_;

	long double run_time;
	long double guest_time;
};
} // namespace arancini::runtime::exec
