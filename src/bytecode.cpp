#include "bytecode.h"
#include <cstring>
#include <deque>
#include <limits>

static inline uint32_t loadU32p(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

static int stackEffect(Op op, uint32_t immArgc = 0) {
    switch (op) {
        case Op::NOP: return 0;

        case Op::ICONST: return +1;
        case Op::FCONST: return +1;

        case Op::LOAD:   return +1;
        case Op::STORE:  return -1;

        case Op::IADD:
        case Op::ISUB:
        case Op::IMUL:
        case Op::IDIV:
        case Op::IMOD:
        case Op::CMPLE:
        case Op::CMPLT:
        case Op::CMPGE:
        case Op::CMPGT:
        case Op::CMPEQ:
        case Op::CMPNE:
            return -1;

        case Op::I2F:
        case Op::F2I:
            return 0;

        case Op::FADD:
        case Op::FSUB:
        case Op::FMUL:
        case Op::FDIV:
        case Op::FCMPLE:
        case Op::FCMPLT:
        case Op::FCMPGE:
        case Op::FCMPGT:
        case Op::FCMPEQ:
        case Op::FCMPNE:
            return -1;

        case Op::FSQRT:
            return 0;

        case Op::POP:     return -1;
        case Op::PRINT:   return -1;
        case Op::PRINT_F: return -1;

        case Op::ARRAY_NEW: return 0;
        case Op::ARRAY_GET: return -1;
        case Op::ARRAY_SET: return -3;
        case Op::ARRAY_LEN: return 0;

        case Op::TIME_MS: return +1;
        case Op::RAND:    return +1;

        case Op::JMP:          return 0;
        case Op::JMP_IF_FALSE: return -1;

        case Op::CALL: return -static_cast<int>(immArgc) + 1;
        case Op::RET:  return -1;
        case Op::HALT: return 0;

        case Op::PRINT_BIG: return -2;
    }
    return 0;
}

uint32_t computeMaxStack(const Program& prog, const Function& fn) {
    const auto& code = prog.code.buf;
    const size_t start = fn.entry;
    const size_t end = fn.end;

    std::vector<int> height(code.size(), std::numeric_limits<int>::min());
    std::deque<size_t> q;

    height[start] = 0;
    q.emplace_back(start);

    int best = 0;

    while (!q.empty()) {
        size_t ip = q.front();
        q.pop_front();

        if (ip < start || ip >= end) continue;

        int h = height[ip];
        if (h < 0) continue;

        Op op = static_cast<Op>(code[ip++]);
        uint32_t argc = 0;

        size_t next_ip = ip;
        size_t jmp_target = 0;
        bool has_fallthrough = true;
        bool has_jump = false;
        bool is_end = false;

        switch (op) {
            case Op::ICONST:
            case Op::FCONST:
                next_ip += 8;
                break;

            case Op::LOAD:
            case Op::STORE:
                next_ip += 4;
                break;

            case Op::JMP: {
                jmp_target = loadU32p(&code[next_ip]);
                next_ip += 4;
                has_jump = true;
                has_fallthrough = false;
                break;
            }

            case Op::JMP_IF_FALSE: {
                jmp_target = loadU32p(&code[next_ip]);
                next_ip += 4;
                has_jump = true;
                break;
            }

            case Op::CALL: {
                (void)loadU32p(&code[next_ip]);
                next_ip += 4;
                argc = loadU32p(&code[next_ip]);
                next_ip += 4;
                break;
            }

            case Op::RET:
            case Op::HALT:
                is_end = true;
                break;

            default:
                break;
        }

        int h2 = h + stackEffect(op, argc);
        if (h2 < 0) h2 = 0;
        if (h2 > best) best = h2;

        if (!is_end && has_fallthrough) {
            if (next_ip < end && height[next_ip] < h2) {
                height[next_ip] = h2;
                q.emplace_back(next_ip);
            }
        }

        if (!is_end && has_jump) {
            if (jmp_target < code.size() && height[jmp_target] < h2) {
                height[jmp_target] = h2;
                q.emplace_back(jmp_target);
            }
        }
    }

    return static_cast<uint32_t>(best + 8);
}
