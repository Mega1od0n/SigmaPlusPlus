#pragma once

#include "bytecode.h"
#include "jit.h"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct VM {
    const Program* prog = nullptr;

    std::vector<int64_t> estack;

    struct Frame {
        uint32_t func_id;
        size_t ip;
        size_t bp;
        uint32_t nlocals;
    };

    std::vector<Frame> callstack;

    struct Array {
        std::vector<int64_t> data;
        bool marked = false;
    };

    std::vector<Array> arrays;
    std::vector<size_t> freeList;

    size_t allocCount = 0;
    size_t gcThreshold = 100;

    struct RootStack {
        int64_t* base;
        size_t* size;
    };

    std::vector<RootStack> rootStacks;

    std::unique_ptr<JITCompiler> jit;

    explicit VM(const Program* p) : prog(p), jit(new JITCompiler()) {}

    int64_t run(const std::string& entryName);

    void runGC();
    void pushFrame(uint32_t fid, size_t ret_ip);
    void popFrame();

    static bool isArrayHandle(int64_t v, size_t arraysSize) {
        if (v >= 0) return false;
        uint64_t id = static_cast<uint64_t>(-(v + 1));
        return id < arraysSize;
    }

    static size_t handleToId(int64_t v) {
        return static_cast<size_t>(-(v + 1));
    }

    static int64_t idToHandle(size_t id) {
        return -static_cast<int64_t>(id + 1);
    }

private:
    int64_t readI64(size_t& ip) const;
    uint32_t readU32(size_t& ip) const;
};
