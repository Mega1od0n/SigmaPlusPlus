#include "vm.h"
#include "runtime.h"
#include "gc.h"
#include <cstring>
#include <stdexcept>

static inline int64_t loadI64(const uint8_t* p) {
    int64_t v;
    std::memcpy(&v, p, 8);
    return v;
}

static inline uint32_t loadU32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

static inline double bitsToDouble(int64_t bits) {
    double d = 0.0;
    std::memcpy(&d, &bits, sizeof(double));
    return d;
}

static inline int64_t doubleToBits(double d) {
    int64_t bits = 0;
    std::memcpy(&bits, &d, sizeof(double));
    return bits;
}

int64_t VM::readI64(size_t& ip) const {
    auto v = loadI64(&prog->code.buf[ip]);
    ip += 8;
    return v;
}

uint32_t VM::readU32(size_t& ip) const {
    auto v = loadU32(&prog->code.buf[ip]);
    ip += 4;
    return v;
}

void VM::pushFrame(uint32_t fid, size_t ret_ip) {
    const Function& f = prog->funcs[fid];

    if (estack.size() < f.arity) {
        throw std::runtime_error("CALL: not enough arguments for function " + f.name);
    }

    size_t base = estack.size() - f.arity;
    if (f.nlocals > f.arity) {
        uint32_t need = f.nlocals - f.arity;
        for (uint32_t i = 0; i < need; ++i) estack.emplace_back(0);
    }

    callstack.emplace_back(Frame{fid, ret_ip, base, f.nlocals});
}

void VM::popFrame() {
    if (callstack.empty()) throw std::runtime_error("RET: no frame");

    auto fr = callstack.back();
    callstack.pop_back();

    if (estack.empty()) throw std::runtime_error("RET: empty stack");

    int64_t ret = estack.back();
    estack.pop_back();

    estack.resize(fr.bp);
    estack.emplace_back(ret);
}

int64_t VM::run(const std::string& entryName) {
    auto it = prog->name2id.find(entryName);
    if (it == prog->name2id.end()) {
        throw std::runtime_error("entry function '" + entryName + "' not found");
    }

    int entryId = static_cast<int>(it->second);

    if (jit) {
        for (uint32_t i = 0; i < prog->funcs.size(); ++i) {
            jit->compileFunction(*prog, i);
        }
    }

    size_t ip = 0;
    estack.clear();
    callstack.clear();

    pushFrame(static_cast<uint32_t>(entryId), SIZE_MAX);
    ip = prog->funcs[entryId].entry;

    auto& code = prog->code.buf;

    for (;;) {
        Op op = static_cast<Op>(code[ip++]);
        switch (op) {
            case Op::NOP:
                break;

            case Op::ICONST: {
                int64_t v = readI64(ip);
                estack.emplace_back(v);
                break;
            }

            case Op::FCONST: {
                int64_t bits = readI64(ip);
                estack.emplace_back(bits);
                break;
            }

            case Op::LOAD: {
                uint32_t slot = readU32(ip);
                auto fr = callstack.back();
                size_t idx = fr.bp + slot;
                if (idx >= estack.size()) throw std::runtime_error("LOAD: slot OOB");
                estack.emplace_back(estack[idx]);
                break;
            }

            case Op::STORE: {
                uint32_t slot = readU32(ip);
                if (estack.empty()) throw std::runtime_error("STORE: empty stack");
                auto v = estack.back();
                estack.pop_back();
                auto fr = callstack.back();
                size_t idx = fr.bp + slot;
                if (idx >= estack.size()) throw std::runtime_error("STORE: slot OOB");
                estack[idx] = v;
                break;
            }

            case Op::IADD: {
                if (estack.size() < 2) throw std::runtime_error("IADD: stack underflow");
                auto b = estack.back(); estack.pop_back();
                auto a = estack.back(); estack.pop_back();
                estack.emplace_back(a + b);
                break;
            }

            case Op::ISUB: {
                if (estack.size() < 2) throw std::runtime_error("ISUB: stack underflow");
                auto b = estack.back(); estack.pop_back();
                auto a = estack.back(); estack.pop_back();
                estack.emplace_back(a - b);
                break;
            }

            case Op::IMUL: {
                if (estack.size() < 2) throw std::runtime_error("IMUL: stack underflow");
                auto b = estack.back(); estack.pop_back();
                auto a = estack.back(); estack.pop_back();
                estack.emplace_back(a * b);
                break;
            }

            case Op::IDIV: {
                if (estack.size() < 2) throw std::runtime_error("IDIV: stack underflow");
                auto b = estack.back(); estack.pop_back();
                auto a = estack.back(); estack.pop_back();
                if (b == 0) throw std::runtime_error("division by zero");
                estack.emplace_back(a / b);
                break;
            }

            case Op::IMOD: {
                if (estack.size() < 2) throw std::runtime_error("IMOD: stack underflow");
                auto b = estack.back(); estack.pop_back();
                auto a = estack.back(); estack.pop_back();
                if (b == 0) throw std::runtime_error("mod by zero");
                estack.emplace_back(a % b);
                break;
            }

            case Op::I2F: {
                if (estack.empty()) throw std::runtime_error("I2F: stack underflow");
                int64_t a = estack.back(); estack.pop_back();
                estack.emplace_back(doubleToBits(static_cast<double>(a)));
                break;
            }

            case Op::F2I: {
                if (estack.empty()) throw std::runtime_error("F2I: stack underflow");
                int64_t bits = estack.back(); estack.pop_back();
                double x = bitsToDouble(bits);
                estack.emplace_back(static_cast<int64_t>(x));
                break;
            }

            case Op::FADD: {
                if (estack.size() < 2) throw std::runtime_error("FADD: stack underflow");
                int64_t bBits = estack.back(); estack.pop_back();
                int64_t aBits = estack.back(); estack.pop_back();
                double a = bitsToDouble(aBits);
                double b = bitsToDouble(bBits);
                estack.emplace_back(doubleToBits(a + b));
                break;
            }

            case Op::FSUB: {
                if (estack.size() < 2) throw std::runtime_error("FSUB: stack underflow");
                int64_t bBits = estack.back(); estack.pop_back();
                int64_t aBits = estack.back(); estack.pop_back();
                double a = bitsToDouble(aBits);
                double b = bitsToDouble(bBits);
                estack.emplace_back(doubleToBits(a - b));
                break;
            }

            case Op::FMUL: {
                if (estack.size() < 2) throw std::runtime_error("FMUL: stack underflow");
                int64_t bBits = estack.back(); estack.pop_back();
                int64_t aBits = estack.back(); estack.pop_back();
                double a = bitsToDouble(aBits);
                double b = bitsToDouble(bBits);
                estack.emplace_back(doubleToBits(a * b));
                break;
            }

            case Op::FDIV: {
                if (estack.size() < 2) throw std::runtime_error("FDIV: stack underflow");
                int64_t bBits = estack.back(); estack.pop_back();
                int64_t aBits = estack.back(); estack.pop_back();
                double a = bitsToDouble(aBits);
                double b = bitsToDouble(bBits);
                estack.emplace_back(doubleToBits(a / b));
                break;
            }

            case Op::FSQRT: {
                if (estack.empty()) throw std::runtime_error("FSQRT: stack underflow");
                int64_t xBits = estack.back(); estack.pop_back();
                int64_t yBits = runtime_sqrt_bits(xBits);
                estack.emplace_back(yBits);
                break;
            }

            case Op::CMPLE: {
                if (estack.size() < 2) throw std::runtime_error("CMPLE: stack underflow");
                auto b = estack.back(); estack.pop_back();
                auto a = estack.back(); estack.pop_back();
                estack.emplace_back(a <= b ? 1 : 0);
                break;
            }

            case Op::CMPLT: {
                if (estack.size() < 2) throw std::runtime_error("CMPLT: stack underflow");
                auto b = estack.back(); estack.pop_back();
                auto a = estack.back(); estack.pop_back();
                estack.emplace_back(a < b ? 1 : 0);
                break;
            }

            case Op::CMPGE: {
                if (estack.size() < 2) throw std::runtime_error("CMPGE: stack underflow");
                auto b = estack.back(); estack.pop_back();
                auto a = estack.back(); estack.pop_back();
                estack.emplace_back(a >= b ? 1 : 0);
                break;
            }

            case Op::CMPGT: {
                if (estack.size() < 2) throw std::runtime_error("CMPGT: stack underflow");
                auto b = estack.back(); estack.pop_back();
                auto a = estack.back(); estack.pop_back();
                estack.emplace_back(a > b ? 1 : 0);
                break;
            }

            case Op::CMPEQ: {
                if (estack.size() < 2) throw std::runtime_error("CMPEQ: stack underflow");
                auto b = estack.back(); estack.pop_back();
                auto a = estack.back(); estack.pop_back();
                estack.emplace_back(a == b ? 1 : 0);
                break;
            }

            case Op::CMPNE: {
                if (estack.size() < 2) throw std::runtime_error("CMPNE: stack underflow");
                auto b = estack.back(); estack.pop_back();
                auto a = estack.back(); estack.pop_back();
                estack.emplace_back(a != b ? 1 : 0);
                break;
            }

            case Op::FCMPLE: {
                if (estack.size() < 2) throw std::runtime_error("FCMPLE: stack underflow");
                int64_t bBits = estack.back(); estack.pop_back();
                int64_t aBits = estack.back(); estack.pop_back();
                double a = bitsToDouble(aBits);
                double b = bitsToDouble(bBits);
                estack.emplace_back(a <= b ? 1 : 0);
                break;
            }

            case Op::FCMPLT: {
                if (estack.size() < 2) throw std::runtime_error("FCMPLT: stack underflow");
                int64_t bBits = estack.back(); estack.pop_back();
                int64_t aBits = estack.back(); estack.pop_back();
                double a = bitsToDouble(aBits);
                double b = bitsToDouble(bBits);
                estack.emplace_back(a < b ? 1 : 0);
                break;
            }

            case Op::FCMPGE: {
                if (estack.size() < 2) throw std::runtime_error("FCMPGE: stack underflow");
                int64_t bBits = estack.back(); estack.pop_back();
                int64_t aBits = estack.back(); estack.pop_back();
                double a = bitsToDouble(aBits);
                double b = bitsToDouble(bBits);
                estack.emplace_back(a >= b ? 1 : 0);
                break;
            }

            case Op::FCMPGT: {
                if (estack.size() < 2) throw std::runtime_error("FCMPGT: stack underflow");
                int64_t bBits = estack.back(); estack.pop_back();
                int64_t aBits = estack.back(); estack.pop_back();
                double a = bitsToDouble(aBits);
                double b = bitsToDouble(bBits);
                estack.emplace_back(a > b ? 1 : 0);
                break;
            }

            case Op::FCMPEQ: {
                if (estack.size() < 2) throw std::runtime_error("FCMPEQ: stack underflow");
                int64_t bBits = estack.back(); estack.pop_back();
                int64_t aBits = estack.back(); estack.pop_back();
                double a = bitsToDouble(aBits);
                double b = bitsToDouble(bBits);
                estack.emplace_back(a == b ? 1 : 0);
                break;
            }

            case Op::FCMPNE: {
                if (estack.size() < 2) throw std::runtime_error("FCMPNE: stack underflow");
                int64_t bBits = estack.back(); estack.pop_back();
                int64_t aBits = estack.back(); estack.pop_back();
                double a = bitsToDouble(aBits);
                double b = bitsToDouble(bBits);
                estack.emplace_back(a != b ? 1 : 0);
                break;
            }

            case Op::JMP: {
                uint32_t addr = readU32(ip);
                ip = addr;
                break;
            }

            case Op::JMP_IF_FALSE: {
                uint32_t addr = readU32(ip);
                if (estack.empty()) throw std::runtime_error("JMP_IF_FALSE: empty stack");
                auto cond = estack.back();
                estack.pop_back();
                if (!cond) ip = addr;
                break;
            }

            case Op::CALL: {
                uint32_t fid = readU32(ip);
                uint32_t argc = readU32(ip);

                if (jit && jit->isCompiled(fid)) {
                    if (estack.size() < argc) throw std::runtime_error("CALL: not enough args");
                    int64_t* argsPtr = estack.data() + (estack.size() - argc);
                    int64_t res = runtime_call_function(this, fid, argsPtr, argc);
                    estack.resize(estack.size() - argc);
                    estack.emplace_back(res);
                    break;
                }

                size_t ret_ip = ip;
                pushFrame(fid, ret_ip);
                ip = prog->funcs[fid].entry;
                break;
            }

            case Op::RET: {
                if (callstack.empty()) throw std::runtime_error("RET: no frame");
                size_t ret_to = callstack.back().ip;
                popFrame();
                if (ret_to == SIZE_MAX) {
                    return estack.empty() ? 0 : estack.back();
                }
                ip = ret_to;
                break;
            }

            case Op::POP: {
                if (estack.empty()) throw std::runtime_error("POP: empty stack");
                estack.pop_back();
                break;
            }

            case Op::PRINT: {
                if (estack.empty()) throw std::runtime_error("PRINT: empty stack");
                auto v = estack.back();
                estack.pop_back();
                runtime_print(v);
                break;
            }

            case Op::PRINT_F: {
                if (estack.empty()) throw std::runtime_error("PRINT_F: empty stack");
                auto bits = estack.back();
                estack.pop_back();
                runtime_print_f_bits(bits);
                break;
            }

            case Op::HALT:
                return estack.empty() ? 0 : estack.back();

            case Op::ARRAY_NEW: {
                if (estack.empty()) throw std::runtime_error("ARRAY_NEW: empty stack");
                int64_t size = estack.back();
                estack.pop_back();
                int64_t handle = runtime_array_new(this, size);
                estack.emplace_back(handle);
                break;
            }

            case Op::ARRAY_GET: {
                if (estack.size() < 2) throw std::runtime_error("ARRAY_GET: stack underflow");
                int64_t idx = estack.back(); estack.pop_back();
                int64_t handle = estack.back(); estack.pop_back();
                int64_t val = runtime_array_get(this, handle, idx);
                estack.emplace_back(val);
                break;
            }

            case Op::ARRAY_SET: {
                if (estack.size() < 3) throw std::runtime_error("ARRAY_SET: stack underflow");
                int64_t val = estack.back(); estack.pop_back();
                int64_t idx = estack.back(); estack.pop_back();
                int64_t handle = estack.back(); estack.pop_back();
                runtime_array_set(this, handle, idx, val);
                break;
            }

            case Op::ARRAY_LEN: {
                if (estack.empty()) throw std::runtime_error("ARRAY_LEN: empty stack");
                int64_t handle = estack.back();
                estack.pop_back();
                int64_t len = runtime_array_len(this, handle);
                estack.emplace_back(len);
                break;
            }

            case Op::TIME_MS:
                estack.emplace_back(runtime_time_ms());
                break;

            case Op::PRINT_BIG: {
                if (estack.size() < 2) throw std::runtime_error("PRINT_BIG: stack underflow");
                int64_t len = estack.back(); estack.pop_back();
                int64_t handle = estack.back(); estack.pop_back();
                runtime_print_big(this, handle, len);
                break;
            }

            case Op::RAND:
                estack.emplace_back(runtime_rand());
                break;

            default:
                throw std::runtime_error("unknown opcode");
        }
    }
}

void VM::runGC() {
    GC::runGC(this);
}
