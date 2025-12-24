#include "gc.h"
#include "vm.h"
#include <vector>

namespace GC {

    void markReachable(VM* vm) {
        for (auto& arr : vm->arrays) {
            arr.marked = false;
        }

        std::vector<size_t> work;

        auto markFromHandle = [&](int64_t v) {
            if (!VM::isArrayHandle(v, vm->arrays.size())) return;

            size_t id = VM::handleToId(v);
            if (vm->arrays[id].marked) return;

            vm->arrays[id].marked = true;
            work.emplace_back(id);
        };

        for (int64_t val : vm->estack) {
            markFromHandle(val);
        }

        for (const auto& rootStack : vm->rootStacks) {
            size_t size = *rootStack.size;
            for (size_t i = 0; i < size; ++i) {
                markFromHandle(rootStack.base[i]);
            }
        }

        while (!work.empty()) {
            size_t id = work.back();
            work.pop_back();

            auto& data = vm->arrays[id].data;
            for (int64_t v : data) {
                markFromHandle(v);
            }
        }
    }

    void sweep(VM* vm) {
        for (size_t i = 0; i < vm->arrays.size(); ++i) {
            if (!vm->arrays[i].marked && !vm->arrays[i].data.empty()) {
                vm->arrays[i].data.clear();
                vm->arrays[i].data.shrink_to_fit();
                vm->freeList.emplace_back(i);
            }
        }
    }

    void runGC(VM* vm) {
        markReachable(vm);
        sweep(vm);
    }
}
