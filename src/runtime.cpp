#include "runtime.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>

void runtime_print(int64_t v) {
    std::cout << v << std::endl;
}

void runtime_print_f_bits(int64_t bits) {
    double x = 0.0;
    std::memcpy(&x, &bits, sizeof(double));

    std::ios::fmtflags f = std::cout.flags();
    std::streamsize p = std::cout.precision();

    std::cout.setf(std::ios::fmtflags(0), std::ios::floatfield);
    std::cout.precision(17);
    std::cout << x << std::endl;

    std::cout.flags(f);
    std::cout.precision(p);
}

int64_t runtime_array_new(VM* vm, int64_t size) {
    if (size < 0) throw std::runtime_error("ARRAY_NEW: negative size");

    vm->allocCount++;
    if (vm->allocCount >= vm->gcThreshold) {
        vm->runGC();
        vm->allocCount = 0;
    }

    size_t arr_id;
    if (!vm->freeList.empty()) {
        arr_id = vm->freeList.back();
        vm->freeList.pop_back();
        vm->arrays[arr_id].data = std::vector<int64_t>(static_cast<size_t>(size), 0);
        vm->arrays[arr_id].marked = false;
    } else {
        arr_id = vm->arrays.size();
        VM::Array arr;
        arr.data = std::vector<int64_t>(static_cast<size_t>(size), 0);
        arr.marked = false;
        vm->arrays.emplace_back(arr);
    }

    return VM::idToHandle(arr_id);
}

int64_t runtime_array_get(VM* vm, int64_t handle, int64_t idx) {
    if (!VM::isArrayHandle(handle, vm->arrays.size())) {
        throw std::runtime_error("ARRAY_GET: invalid array handle");
    }

    size_t arr_id = VM::handleToId(handle);
    auto& arr = vm->arrays[arr_id].data;

    if (idx < 0 || static_cast<size_t>(idx) >= arr.size()) {
        throw std::runtime_error("ARRAY_GET: index out of bounds");
    }

    return arr[static_cast<size_t>(idx)];
}

void runtime_array_set(VM* vm, int64_t handle, int64_t idx, int64_t val) {
    if (!VM::isArrayHandle(handle, vm->arrays.size())) {
        throw std::runtime_error("ARRAY_SET: invalid array handle");
    }

    size_t arr_id = VM::handleToId(handle);
    auto& arr = vm->arrays[arr_id].data;

    if (idx < 0 || static_cast<size_t>(idx) >= arr.size()) {
        throw std::runtime_error("ARRAY_SET: index out of bounds");
    }

    arr[static_cast<size_t>(idx)] = val;
}

int64_t runtime_array_len(VM* vm, int64_t handle) {
    if (!VM::isArrayHandle(handle, vm->arrays.size())) {
        throw std::runtime_error("ARRAY_LEN: invalid array handle");
    }

    size_t arr_id = VM::handleToId(handle);
    return static_cast<int64_t>(vm->arrays[arr_id].data.size());
}

int64_t runtime_call_function(VM* vm, uint32_t func_id, int64_t* args, uint32_t argc) {
    if (func_id >= vm->prog->funcs.size()) {
        throw std::runtime_error("CALL: invalid function ID");
    }

    const Function& func = vm->prog->funcs[func_id];

    if (!(vm->jit && vm->jit->isCompiled(func_id))) {
        throw std::runtime_error(
                "runtime_call_function: function '" + func.name +
                "' is not compiled by JIT; this should not happen as all functions are pre-compiled"
        );
    }

    auto jitFunc = vm->jit->getCompiledFunction(func_id);
    if (!jitFunc) {
        throw std::runtime_error(
                "runtime_call_function: failed to get compiled function '" + func.name + "'"
        );
    }

    std::vector<int64_t> locals(func.nlocals, 0);
    for (uint32_t i = 0; i < argc && i < func.arity; ++i) {
        locals[i] = args[i];
    }

    size_t cap = func.maxStack ? func.maxStack : 1024;
    std::unique_ptr<int64_t[]> stack(new int64_t[cap]);

    JITContext ctx;
    ctx.locals = locals.data();
    ctx.stack = stack.get();
    ctx.stack_size = 0;
    ctx.vm = vm;

    size_t locals_size = func.nlocals;

    vm->rootStacks.push_back({locals.data(), &locals_size});
    vm->rootStacks.push_back({stack.get(), &ctx.stack_size});

    int64_t result = jitFunc(&ctx);

    vm->rootStacks.pop_back();
    vm->rootStacks.pop_back();

    return result;
}

int64_t runtime_time_ms() {
    using clock = std::chrono::steady_clock;

    static const auto t0 = clock::now();
    auto now = clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();

    return static_cast<int64_t>(ms);
}

int64_t runtime_rand() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    return static_cast<int64_t>(gen() & 0x7FFFFFFFFFFFFFFFLL);
}

void runtime_print_big(VM* vm, int64_t handle, int64_t len) {
    if (handle >= 0) throw std::runtime_error("PRINT_BIG: invalid array handle");
    size_t id = static_cast<size_t>(-handle - 1);
    if (id >= vm->arrays.size()) throw std::runtime_error("PRINT_BIG: invalid array id");

    auto& a = vm->arrays[id].data;
    if (len < 0) throw std::runtime_error("PRINT_BIG: negative len");
    if (static_cast<size_t>(len) > a.size()) throw std::runtime_error("PRINT_BIG: len out of bounds");

    const int64_t baseDigits = 9;
    int64_t i = len - 1;
    while (i > 0 && a[static_cast<size_t>(i)] == 0) --i;

    std::cout << a[static_cast<size_t>(i)];
    for (i = i - 1; i >= 0; --i) {
        std::cout << std::setw(baseDigits) << std::setfill('0') << a[static_cast<size_t>(i)];
        if (i == 0) break;
    }
    std::cout << std::endl;
}
int64_t runtime_sqrt_bits(int64_t x_bits) {
    double x = 0.0;
    std::memcpy(&x, &x_bits, sizeof(double));
    double y = std::sqrt(x);
    int64_t y_bits = 0;
    std::memcpy(&y_bits, &y, sizeof(double));
    return y_bits;
}
