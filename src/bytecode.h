#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

enum class Op : uint8_t {
    NOP = 0,
    ICONST,
    LOAD,
    STORE,
    IADD,
    ISUB,
    IMUL,
    IDIV,
    IMOD,
    CMPLE,
    CMPLT,
    CMPGE,
    CMPGT,
    CMPEQ,
    CMPNE,
    JMP,
    JMP_IF_FALSE,
    CALL,
    RET,
    POP,
    PRINT,
    HALT,
    ARRAY_NEW,
    ARRAY_GET,
    ARRAY_SET,
    ARRAY_LEN,
    TIME_MS,
    RAND,
    FCONST,
    I2F,
    F2I,
    FADD,
    FSUB,
    FMUL,
    FDIV,
    FCMPLE,
    FCMPLT,
    FCMPGE,
    FCMPGT,
    FCMPEQ,
    FCMPNE,
    FSQRT,
    PRINT_BIG,
    PRINT_F
};

struct Code {
    std::vector<uint8_t> buf;

    size_t pc() const { return buf.size(); }

    void emit8(uint8_t v) { buf.emplace_back(v); }

    void emit64(int64_t v) {
        uint8_t* p = reinterpret_cast<uint8_t*>(&v);
        buf.insert(buf.end(), p, p + 8);
    }

    void emit32(uint32_t v) {
        uint8_t* p = reinterpret_cast<uint8_t*>(&v);
        buf.insert(buf.end(), p, p + 4);
    }

    void op(Op o) { emit8(static_cast<uint8_t>(o)); }

    void i64(int64_t v) { emit64(v); }

    void u32(uint32_t v) { emit32(v); }

    void patch32(size_t at, uint32_t value) {
        uint8_t* p = reinterpret_cast<uint8_t*>(&value);
        for (int i = 0; i < 4; ++i) buf[at + i] = p[i];
    }
};

struct Function {
    std::string name;
    uint32_t id = 0;
    uint32_t arity = 0;
    uint32_t nlocals = 0;
    size_t entry = 0;
    size_t end = 0;
    uint32_t maxStack = 0;
};

struct Program {
    Code code;
    std::vector<Function> funcs;
    std::unordered_map<std::string, uint32_t> name2id;

    struct LoopContext {
        std::vector<size_t> breakPatches;
        std::vector<size_t> continuePatches;
    };

    std::vector<LoopContext> loopStack;

    uint32_t addFunc(const std::string& name, uint32_t arity, uint32_t nlocals, size_t entry) {
        uint32_t id = static_cast<uint32_t>(funcs.size());
        funcs.emplace_back(Function{name, id, arity, nlocals, entry});
        name2id[name] = id;
        return id;
    }

    int findFuncId(const std::string& name) const {
        auto it = name2id.find(name);
        if (it == name2id.end()) return -1;
        return static_cast<int>(it->second);
    }

    void dump() const {
        std::cout << "Functions:\n";
        for (auto& f : funcs) {
            std::cout << "  [" << f.id << "] " << f.name
                      << " arity=" << f.arity
                      << " locals=" << f.nlocals
                      << " entry=" << f.entry << "\n";
        }
        std::cout << "Code size: " << code.buf.size() << " bytes\n";
    }
};

uint32_t computeMaxStack(const Program& prog, const Function& fn);
