#include "jit.h"
#include "runtime.h"
#include <cstring>
#include <deque>
#include <unordered_map>

using namespace asmjit;

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

struct JitInstrInfo {
    size_t ip = 0;
    Op op = Op::NOP;
    int64_t imm64 = 0;
    uint32_t imm0 = 0;
    uint32_t imm1 = 0;
    size_t next_ip = 0;
    size_t jmp_target = 0;
    bool has_jump = false;
    bool has_fallthrough = true;
    bool is_end = false;
    int consume = 0;
    int produce = 0;
    bool side_effect = false;
    bool uses_inputs = true;
    int height_before = -1;
    int height_after = -1;
    bool result_live = true;
};

JITCompiler::JITCompiler() {
}

JITCompiler::~JITCompiler() {
    compiledFunctions.clear();
}

bool JITCompiler::isCompiled(uint32_t funcId) const {
    return compiledFunctions.find(funcId) != compiledFunctions.end();
}

JITCompiler::CompiledFunc JITCompiler::getCompiledFunction(uint32_t funcId) const {
    auto it = compiledFunctions.find(funcId);
    if (it != compiledFunctions.end()) {
        return it->second;
    }
    return nullptr;
}

JITCompiler::CompiledFunc JITCompiler::compileFunction(const Program& prog, uint32_t funcId) {
    if (funcId >= prog.funcs.size()) {
        return nullptr;
    }

    const Function& func = prog.funcs[funcId];
    const auto& code = prog.code.buf;

    size_t func_start = func.entry;
    size_t func_end = func.end;

    size_t ip = func_start;

    std::vector<JitInstrInfo> insts;
    insts.reserve(func_end - func_start);
    std::vector<int> ip_to_index(prog.code.buf.size(), -1);

    while (ip < func_end) {
        JitInstrInfo ins;
        ins.ip = ip;
        ins.op = static_cast<Op>(code[ip++]);

        switch (ins.op) {
            case Op::NOP:
                ins.uses_inputs = false;
                break;

            case Op::ICONST:
            case Op::FCONST:
                ins.imm64 = loadI64(&code[ip]);
                ip += 8;
                ins.produce = 1;
                ins.uses_inputs = false;
                break;

            case Op::LOAD:
                ins.imm0 = loadU32(&code[ip]);
                ip += 4;
                ins.produce = 1;
                ins.uses_inputs = false;
                break;

            case Op::STORE:
                ins.imm0 = loadU32(&code[ip]);
                ip += 4;
                ins.consume = 1;
                ins.side_effect = true;
                break;

            case Op::IADD:
            case Op::ISUB:
            case Op::IMUL:
                ins.consume = 2;
                ins.produce = 1;
                break;

            case Op::IDIV:
            case Op::IMOD:
                ins.consume = 2;
                ins.produce = 1;
                ins.side_effect = true;
                break;

            case Op::I2F:
            case Op::F2I:
            case Op::FSQRT:
                ins.consume = 1;
                ins.produce = 1;
                break;

            case Op::FADD:
            case Op::FSUB:
            case Op::FMUL:
            case Op::FDIV:
                ins.consume = 2;
                ins.produce = 1;
                break;

            case Op::CMPLE:
            case Op::CMPLT:
            case Op::CMPGE:
            case Op::CMPGT:
            case Op::CMPEQ:
            case Op::CMPNE:
            case Op::FCMPLE:
            case Op::FCMPLT:
            case Op::FCMPGE:
            case Op::FCMPGT:
            case Op::FCMPEQ:
            case Op::FCMPNE:
                ins.consume = 2;
                ins.produce = 1;
                break;

            case Op::JMP:
                ins.imm0 = loadU32(&code[ip]);
                ip += 4;
                ins.jmp_target = ins.imm0;
                ins.has_jump = true;
                ins.has_fallthrough = false;
                ins.side_effect = true;
                ins.uses_inputs = false;
                break;

            case Op::JMP_IF_FALSE:
                ins.imm0 = loadU32(&code[ip]);
                ip += 4;
                ins.jmp_target = ins.imm0;
                ins.has_jump = true;
                ins.consume = 1;
                ins.side_effect = true;
                break;

            case Op::CALL:
                ins.imm0 = loadU32(&code[ip]);
                ip += 4;
                ins.imm1 = loadU32(&code[ip]);
                ip += 4;
                ins.consume = static_cast<int>(ins.imm1);
                ins.produce = 1;
                ins.side_effect = true;
                break;

            case Op::RET:
                ins.consume = 1;
                ins.side_effect = true;
                ins.is_end = true;
                ins.has_fallthrough = false;
                break;

            case Op::HALT:
                ins.side_effect = true;
                ins.is_end = true;
                ins.has_fallthrough = false;
                ins.uses_inputs = false;
                break;

            case Op::POP:
                ins.consume = 1;
                ins.uses_inputs = false;
                break;

            case Op::PRINT:
            case Op::PRINT_F:
                ins.consume = 1;
                ins.side_effect = true;
                break;

            case Op::PRINT_BIG:
                ins.consume = 2;
                ins.side_effect = true;
                break;

            case Op::ARRAY_NEW:
                ins.consume = 1;
                ins.produce = 1;
                ins.side_effect = true;
                break;

            case Op::ARRAY_GET:
                ins.consume = 2;
                ins.produce = 1;
                ins.side_effect = true;
                break;

            case Op::ARRAY_SET:
                ins.consume = 3;
                ins.side_effect = true;
                break;

            case Op::ARRAY_LEN:
                ins.consume = 1;
                ins.produce = 1;
                ins.side_effect = true;
                break;

            case Op::TIME_MS:
            case Op::RAND:
                ins.produce = 1;
                ins.side_effect = true;
                ins.uses_inputs = false;
                break;

            default:
                return nullptr;
        }

        ins.next_ip = ip;
        ip_to_index[ins.ip] = static_cast<int>(insts.size());
        insts.emplace_back(ins);
    }

    CodeHolder codeHolder;
    codeHolder.init(runtime.environment(), runtime.cpu_features());

    x86::Assembler a(&codeHolder);

    a.push(x86::rbp);
    a.mov(x86::rbp, x86::rsp);
    a.push(x86::rbx);
    a.push(x86::rdi);
    a.push(x86::r12);
    a.push(x86::r13);
    a.push(x86::r14);
    a.push(x86::r15);

    a.mov(x86::rdi, x86::rcx);

    a.mov(x86::rbx, x86::ptr(x86::rdi, offsetof(JITContext, locals)));
    a.mov(x86::r12, x86::ptr(x86::rdi, offsetof(JITContext, stack)));
    a.xor_(x86::r13, x86::r13);

    std::unordered_map<size_t, Label> labels;

    ip = func_start;
    while (ip < func_end) {
        Op op = static_cast<Op>(code[ip]);
        ip++;

        if (op == Op::JMP || op == Op::JMP_IF_FALSE) {
            uint32_t target = loadU32(&code[ip]);
            if (labels.find(target) == labels.end()) {
                labels[target] = a.new_label();
            }
            ip += 4;
        } else {
            switch (op) {
                case Op::ICONST: ip += 8; break;
                case Op::FCONST: ip += 8; break;
                case Op::LOAD:
                case Op::STORE: ip += 4; break;
                case Op::CALL: ip += 8; break;
                default: break;
            }
        }
    }

    bool dce_enabled = true;
    std::vector<int> height(insts.size(), -1);
    if (!insts.empty()) {
        std::deque<size_t> q;
        height[0] = 0;
        q.emplace_back(0);

        while (!q.empty() && dce_enabled) {
            size_t idx = q.front();
            q.pop_front();

            int h = height[idx];
            if (h < 0) continue;
            int h2 = h + (insts[idx].produce - insts[idx].consume);
            if (h2 < 0) {
                dce_enabled = false;
                break;
            }

            auto add_succ = [&](size_t target_ip) {
                if (target_ip >= func_end) return;
                if (target_ip >= ip_to_index.size()) {
                    dce_enabled = false;
                    return;
                }
                int t = ip_to_index[target_ip];
                if (t < 0) {
                    dce_enabled = false;
                    return;
                }
                if (height[t] == -1) {
                    height[t] = h2;
                    q.emplace_back(static_cast<size_t>(t));
                } else if (height[t] != h2) {
                    dce_enabled = false;
                }
            };

            if (!insts[idx].is_end && insts[idx].has_fallthrough) {
                add_succ(insts[idx].next_ip);
            }
            if (!insts[idx].is_end && insts[idx].has_jump) {
                add_succ(insts[idx].jmp_target);
            }
        }
    }

    if (dce_enabled) {
        for (size_t i = 0; i < insts.size(); ++i) {
            if (height[i] < 0) continue;
            insts[i].height_before = height[i];
            insts[i].height_after = height[i] + (insts[i].produce - insts[i].consume);
            if (insts[i].height_after < 0) {
                dce_enabled = false;
                break;
            }
        }
    }

    std::vector<std::vector<uint8_t>> live_in;
    std::vector<std::vector<uint8_t>> live_out;
    std::vector<std::vector<size_t>> preds;
    std::vector<std::vector<size_t>> succs;

    if (dce_enabled) {
        live_in.resize(insts.size());
        live_out.resize(insts.size());
        preds.resize(insts.size());
        succs.resize(insts.size());

        for (size_t i = 0; i < insts.size(); ++i) {
            if (insts[i].height_before < 0) continue;
            live_in[i].assign(static_cast<size_t>(insts[i].height_before), 0);
            live_out[i].assign(static_cast<size_t>(insts[i].height_after), 0);
        }

        auto add_edge = [&](size_t from, size_t to) {
            if (insts[from].height_before < 0 || insts[to].height_before < 0) return;
            if (insts[from].height_after != insts[to].height_before) {
                dce_enabled = false;
                return;
            }
            succs[from].emplace_back(to);
            preds[to].emplace_back(from);
        };

        for (size_t i = 0; i < insts.size() && dce_enabled; ++i) {
            if (insts[i].height_before < 0 || insts[i].is_end) continue;
            if (insts[i].has_fallthrough) {
                size_t nip = insts[i].next_ip;
                if (nip < ip_to_index.size()) {
                    int ni = ip_to_index[nip];
                    if (ni >= 0) add_edge(i, static_cast<size_t>(ni));
                }
            }
            if (insts[i].has_jump) {
                size_t tip = insts[i].jmp_target;
                if (tip < ip_to_index.size()) {
                    int ti = ip_to_index[tip];
                    if (ti >= 0) add_edge(i, static_cast<size_t>(ti));
                }
            }
        }

        if (dce_enabled) {
            std::deque<size_t> wl;
            for (size_t i = 0; i < insts.size(); ++i) {
                if (insts[i].height_before >= 0) wl.emplace_back(i);
            }

            while (!wl.empty() && dce_enabled) {
                size_t i = wl.front();
                wl.pop_front();

                if (insts[i].height_before < 0) continue;

                std::vector<uint8_t> new_out(static_cast<size_t>(insts[i].height_after), 0);
                for (size_t s = 0; s < succs[i].size(); ++s) {
                    size_t succ = succs[i][s];
                    const auto& lin = live_in[succ];
                    for (size_t k = 0; k < new_out.size() && k < lin.size(); ++k) {
                        new_out[k] = static_cast<uint8_t>(new_out[k] | lin[k]);
                    }
                }

                if (new_out != live_out[i]) {
                    live_out[i] = new_out;
                }

                std::vector<uint8_t> new_in(static_cast<size_t>(insts[i].height_before), 0);
                if (insts[i].op == Op::HALT) {
                    if (insts[i].height_before > 0) {
                        new_in[static_cast<size_t>(insts[i].height_before - 1)] = 1;
                    }
                } else {
                    int c = insts[i].consume;
                    int p = insts[i].produce;
                    int base = insts[i].height_before - c;
                    if (base < 0) {
                        dce_enabled = false;
                        break;
                    }
                    for (int k = 0; k < base; ++k) {
                        new_in[static_cast<size_t>(k)] = live_out[i][static_cast<size_t>(k)];
                    }
                    bool result_live = false;
                    for (int k = 0; k < p; ++k) {
                        if (live_out[i][static_cast<size_t>(base + k)]) {
                            result_live = true;
                            break;
                        }
                    }
                    bool needed = insts[i].side_effect || result_live;
                    if (needed && insts[i].uses_inputs) {
                        for (int k = 0; k < c; ++k) {
                            new_in[static_cast<size_t>(base + k)] = 1;
                        }
                    }
                }

                if (new_in != live_in[i]) {
                    live_in[i] = new_in;
                    for (size_t p = 0; p < preds[i].size(); ++p) {
                        wl.emplace_back(preds[i][p]);
                    }
                }
            }
        }

        if (dce_enabled) {
            for (size_t i = 0; i < insts.size(); ++i) {
                if (insts[i].height_before < 0) continue;
                if (insts[i].produce <= 0) {
                    insts[i].result_live = false;
                    continue;
                }
                int base = insts[i].height_before - insts[i].consume;
                if (base < 0) {
                    dce_enabled = false;
                    break;
                }
                bool live = false;
                for (int k = 0; k < insts[i].produce; ++k) {
                    if (live_out[i][static_cast<size_t>(base + k)]) {
                        live = true;
                        break;
                    }
                }
                insts[i].result_live = live;
            }
        }
    }

    if (!dce_enabled) {
        for (auto& ins : insts) {
            ins.result_live = true;
        }
    }

    auto adjust_stack = [&](int delta) {
        if (delta > 0) {
            if (delta == 1) a.inc(x86::r13);
            else a.add(x86::r13, delta);
        } else if (delta < 0) {
            if (delta == -1) a.dec(x86::r13);
            else a.sub(x86::r13, -delta);
        }
    };

    ip = func_start;
    while (ip < func_end) {
        size_t cur_ip = ip;

        auto itLab = labels.find(cur_ip);
        if (itLab != labels.end()) {
            a.bind(itLab->second);
        }

        int ins_index = (cur_ip < ip_to_index.size()) ? ip_to_index[cur_ip] : -1;
        const JitInstrInfo* ins = (ins_index >= 0) ? &insts[static_cast<size_t>(ins_index)] : nullptr;
        bool need_value = true;
        bool need_exec = true;
        if (ins) {
            need_value = ins->produce > 0 ? ins->result_live : false;
            need_exec = ins->side_effect || need_value;
            if (!need_exec) {
                if (ins->produce > 0) {
                    if (ins->consume == 1) {
                        a.dec(x86::r13);
                    } else if (ins->consume > 1) {
                        a.sub(x86::r13, ins->consume);
                    }
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), 0);
                    if (ins->produce == 1) {
                        a.inc(x86::r13);
                    } else if (ins->produce > 1) {
                        a.add(x86::r13, ins->produce);
                    }
                } else {
                    adjust_stack(ins->produce - ins->consume);
                }
                ip = ins->next_ip;
                continue;
            }
        }

        Op op = static_cast<Op>(code[ip++]);

        switch (op) {
            case Op::NOP:
                break;

            case Op::ICONST: {
                int64_t val = loadI64(&code[ip]);
                ip += 8;
                a.mov(x86::rax, val);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::FCONST: {
                int64_t bits = loadI64(&code[ip]);
                ip += 8;
                a.mov(x86::rax, bits);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::LOAD: {
                uint32_t slot = loadU32(&code[ip]);
                ip += 4;
                a.mov(x86::rax, x86::ptr(x86::rbx, slot * 8));
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::STORE: {
                uint32_t slot = loadU32(&code[ip]);
                ip += 4;
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.mov(x86::ptr(x86::rbx, slot * 8), x86::rax);
                break;
            }

            case Op::IADD: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.add(x86::rax, x86::rdx);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::ISUB: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.sub(x86::rax, x86::rdx);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::IMUL: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.imul(x86::rax, x86::rdx);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::IDIV: {
                a.dec(x86::r13);
                a.mov(x86::rcx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.cqo();
                a.idiv(x86::rcx);
                if (need_value) {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                } else {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), 0);
                }
                a.inc(x86::r13);
                break;
            }

            case Op::IMOD: {
                a.dec(x86::r13);
                a.mov(x86::rcx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.cqo();
                a.idiv(x86::rcx);
                if (need_value) {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rdx);
                } else {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), 0);
                }
                a.inc(x86::r13);
                break;
            }

            case Op::I2F: {
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.cvtsi2sd(x86::xmm0, x86::rax);
                a.movq(x86::rax, x86::xmm0);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::F2I: {
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.movq(x86::xmm0, x86::rax);
                a.cvttsd2si(x86::rax, x86::xmm0);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::FADD: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.movq(x86::xmm0, x86::rax);
                a.movq(x86::xmm1, x86::rdx);
                a.addsd(x86::xmm0, x86::xmm1);
                a.movq(x86::rax, x86::xmm0);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::FSUB: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.movq(x86::xmm0, x86::rax);
                a.movq(x86::xmm1, x86::rdx);
                a.subsd(x86::xmm0, x86::xmm1);
                a.movq(x86::rax, x86::xmm0);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::FMUL: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.movq(x86::xmm0, x86::rax);
                a.movq(x86::xmm1, x86::rdx);
                a.mulsd(x86::xmm0, x86::xmm1);
                a.movq(x86::rax, x86::xmm0);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::FDIV: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.movq(x86::xmm0, x86::rax);
                a.movq(x86::xmm1, x86::rdx);
                a.divsd(x86::xmm0, x86::xmm1);
                a.movq(x86::rax, x86::xmm0);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::FSQRT: {
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.movq(x86::xmm0, x86::rax);
                a.sqrtsd(x86::xmm0, x86::xmm0);
                a.movq(x86::rax, x86::xmm0);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::CMPLE: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.cmp(x86::rax, x86::rdx);
                a.setle(x86::al);
                a.movzx(x86::rax, x86::al);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::CMPLT: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.cmp(x86::rax, x86::rdx);
                a.setl(x86::al);
                a.movzx(x86::rax, x86::al);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::CMPGE: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.cmp(x86::rax, x86::rdx);
                a.setge(x86::al);
                a.movzx(x86::rax, x86::al);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::CMPGT: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.cmp(x86::rax, x86::rdx);
                a.setg(x86::al);
                a.movzx(x86::rax, x86::al);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::CMPEQ: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.cmp(x86::rax, x86::rdx);
                a.sete(x86::al);
                a.movzx(x86::rax, x86::al);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::CMPNE: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.cmp(x86::rax, x86::rdx);
                a.setne(x86::al);
                a.movzx(x86::rax, x86::al);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::FCMPLE: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.movq(x86::xmm0, x86::rax);
                a.movq(x86::xmm1, x86::rdx);
                a.ucomisd(x86::xmm0, x86::xmm1);
                a.setbe(x86::al);
                a.movzx(x86::rax, x86::al);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::FCMPLT: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.movq(x86::xmm0, x86::rax);
                a.movq(x86::xmm1, x86::rdx);
                a.ucomisd(x86::xmm0, x86::xmm1);
                a.setb(x86::al);
                a.movzx(x86::rax, x86::al);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::FCMPGE: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.movq(x86::xmm0, x86::rax);
                a.movq(x86::xmm1, x86::rdx);
                a.ucomisd(x86::xmm0, x86::xmm1);
                a.setae(x86::al);
                a.movzx(x86::rax, x86::al);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::FCMPGT: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.movq(x86::xmm0, x86::rax);
                a.movq(x86::xmm1, x86::rdx);
                a.ucomisd(x86::xmm0, x86::xmm1);
                a.seta(x86::al);
                a.movzx(x86::rax, x86::al);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::FCMPEQ: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.movq(x86::xmm0, x86::rax);
                a.movq(x86::xmm1, x86::rdx);
                a.ucomisd(x86::xmm0, x86::xmm1);
                a.sete(x86::al);
                a.movzx(x86::rax, x86::al);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::FCMPNE: {
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.movq(x86::xmm0, x86::rax);
                a.movq(x86::xmm1, x86::rdx);
                a.ucomisd(x86::xmm0, x86::xmm1);
                a.setne(x86::al);
                a.movzx(x86::rax, x86::al);
                a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                a.inc(x86::r13);
                break;
            }

            case Op::JMP: {
                uint32_t target = loadU32(&code[ip]);
                ip += 4;
                a.jmp(labels[target]);
                break;
            }

            case Op::JMP_IF_FALSE: {
                uint32_t target = loadU32(&code[ip]);
                ip += 4;
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.test(x86::rax, x86::rax);
                a.jz(labels[target]);
                break;
            }

            case Op::POP: {
                a.dec(x86::r13);
                break;
            }

            case Op::PRINT: {
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.mov(x86::rcx, x86::rax);
                a.sub(x86::rsp, 32);
                a.call(imm(reinterpret_cast<uint64_t>(runtime_print)));
                a.add(x86::rsp, 32);
                break;
            }

            case Op::PRINT_F: {
                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));
                a.mov(x86::rcx, x86::rax);
                a.sub(x86::rsp, 32);
                a.call(imm(reinterpret_cast<uint64_t>(runtime_print_f_bits)));
                a.add(x86::rsp, 32);
                break;
            }

            case Op::PRINT_BIG: {
                a.dec(x86::r13);
                a.mov(x86::r8, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.mov(x86::rcx, x86::ptr(x86::rdi, offsetof(JITContext, vm)));
                a.sub(x86::rsp, 32);
                a.call(imm(reinterpret_cast<uint64_t>(runtime_print_big)));
                a.add(x86::rsp, 32);
                break;
            }

            case Op::CALL: {
                uint32_t fid = loadU32(&code[ip]);
                ip += 4;
                uint32_t argc = loadU32(&code[ip]);
                ip += 4;

                a.push(x86::r14);

                a.mov(x86::r14, x86::r13);
                a.sub(x86::r14, argc);
                a.shl(x86::r14, 3);
                a.add(x86::r14, x86::r12);

                a.mov(x86::ptr(x86::rdi, offsetof(JITContext, stack_size)), x86::r13);

                a.mov(x86::rcx, x86::ptr(x86::rdi, offsetof(JITContext, vm)));
                a.mov(x86::edx, fid);
                a.mov(x86::r8, x86::r14);
                a.mov(x86::r9d, argc);
                a.sub(x86::rsp, 40);
                a.call(imm(reinterpret_cast<uint64_t>(runtime_call_function)));
                a.add(x86::rsp, 40);

                a.pop(x86::r14);

                a.sub(x86::r13, argc);
                if (need_value) {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                } else {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), 0);
                }
                a.inc(x86::r13);
                break;
            }

            case Op::ARRAY_NEW: {
                a.dec(x86::r13);
                a.mov(x86::r14, x86::ptr(x86::r12, x86::r13, 3));

                a.mov(x86::ptr(x86::rdi, offsetof(JITContext, stack_size)), x86::r13);

                a.mov(x86::rcx, x86::ptr(x86::rdi, offsetof(JITContext, vm)));
                a.mov(x86::rdx, x86::r14);
                a.sub(x86::rsp, 32);
                a.call(imm(reinterpret_cast<uint64_t>(runtime_array_new)));
                a.add(x86::rsp, 32);

                if (need_value) {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                } else {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), 0);
                }
                a.inc(x86::r13);
                break;
            }

            case Op::ARRAY_GET: {
                a.dec(x86::r13);
                a.mov(x86::r15, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::r14, x86::ptr(x86::r12, x86::r13, 3));

                a.mov(x86::rcx, x86::ptr(x86::rdi, offsetof(JITContext, vm)));
                a.mov(x86::rdx, x86::r14);
                a.mov(x86::r8, x86::r15);
                a.sub(x86::rsp, 32);
                a.call(imm(reinterpret_cast<uint64_t>(runtime_array_get)));
                a.add(x86::rsp, 32);

                if (need_value) {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                } else {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), 0);
                }
                a.inc(x86::r13);
                break;
            }

            case Op::ARRAY_SET: {
                a.dec(x86::r13);
                a.mov(x86::r15, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);
                a.mov(x86::r14, x86::ptr(x86::r12, x86::r13, 3));
                a.dec(x86::r13);

                a.mov(x86::rcx, x86::ptr(x86::rdi, offsetof(JITContext, vm)));
                a.mov(x86::rdx, x86::ptr(x86::r12, x86::r13, 3));
                a.mov(x86::r8, x86::r14);
                a.mov(x86::r9, x86::r15);
                a.sub(x86::rsp, 32);
                a.call(imm(reinterpret_cast<uint64_t>(runtime_array_set)));
                a.add(x86::rsp, 32);
                break;
            }

            case Op::ARRAY_LEN: {
                a.dec(x86::r13);
                a.mov(x86::r14, x86::ptr(x86::r12, x86::r13, 3));

                a.mov(x86::rcx, x86::ptr(x86::rdi, offsetof(JITContext, vm)));
                a.mov(x86::rdx, x86::r14);
                a.sub(x86::rsp, 32);
                a.call(imm(reinterpret_cast<uint64_t>(runtime_array_len)));
                a.add(x86::rsp, 32);

                if (need_value) {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                } else {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), 0);
                }
                a.inc(x86::r13);
                break;
            }

            case Op::TIME_MS: {
                a.sub(x86::rsp, 32);
                a.call(imm(reinterpret_cast<uint64_t>(runtime_time_ms)));
                a.add(x86::rsp, 32);
                if (need_value) {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                } else {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), 0);
                }
                a.inc(x86::r13);
                break;
            }

            case Op::RAND: {
                a.sub(x86::rsp, 32);
                a.call(imm(reinterpret_cast<uint64_t>(runtime_rand)));
                a.add(x86::rsp, 32);
                if (need_value) {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), x86::rax);
                } else {
                    a.mov(x86::ptr(x86::r12, x86::r13, 3), 0);
                }
                a.inc(x86::r13);
                break;
            }

            case Op::RET:
            case Op::HALT: {
                Label empty_stack = a.new_label();

                a.test(x86::r13, x86::r13);
                a.jz(empty_stack);

                a.dec(x86::r13);
                a.mov(x86::rax, x86::ptr(x86::r12, x86::r13, 3));

                a.pop(x86::r15);
                a.pop(x86::r14);
                a.pop(x86::r13);
                a.pop(x86::r12);
                a.pop(x86::rdi);
                a.pop(x86::rbx);
                a.pop(x86::rbp);
                a.ret();

                a.bind(empty_stack);
                a.xor_(x86::rax, x86::rax);

                a.pop(x86::r15);
                a.pop(x86::r14);
                a.pop(x86::r13);
                a.pop(x86::r12);
                a.pop(x86::rdi);
                a.pop(x86::rbx);
                a.pop(x86::rbp);
                a.ret();
                break;
            }

            default:
                return nullptr;
        }
    }

    CompiledFunc fn = nullptr;
    Error err = runtime.add(&fn, &codeHolder);
    if (err != kErrorOk) {
        return nullptr;
    }

    compiledFunctions[funcId] = fn;
    return fn;
}
