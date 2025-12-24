#pragma once
#include "bytecode.h"
#include <asmjit/x86.h>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

struct VM;

struct JITContext {
    int64_t* locals;
    int64_t* stack;
    size_t stack_size;
    VM* vm;
};

class JITCompiler {
public:
    JITCompiler();
    ~JITCompiler();

    typedef int64_t (*CompiledFunc)(JITContext* ctx);
    CompiledFunc compileFunction(const Program& prog, uint32_t funcId);

    bool isCompiled(uint32_t funcId) const;
    CompiledFunc getCompiledFunction(uint32_t funcId) const;

private:
    asmjit::JitRuntime runtime;
    std::unordered_map<uint32_t, CompiledFunc> compiledFunctions;
};
