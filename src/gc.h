#pragma once

#include <cstdint>
#include <vector>

struct VM;

namespace GC {
    void markReachable(VM* vm);
    void sweep(VM* vm);
    void runGC(VM* vm);
}
