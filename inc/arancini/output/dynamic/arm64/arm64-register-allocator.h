#pragma once

#include <arancini/output/dynamic/arm64/arm64-instruction.h>

class physical_register_allocator {
public:
    void allocate(It begin, It end);
};
