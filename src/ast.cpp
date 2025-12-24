#include "ast.h"
#include <stdexcept>

static constexpr int kFloatFlag = (1 << 30);

static inline bool slotIsFloat(int slot) {
    return (slot & kFloatFlag) != 0;
}

static inline uint32_t slotIndex(int slot) {
    return static_cast<uint32_t>(slot & ~kFloatFlag);
}

static inline int slotWithFloat(int slot, bool isFloat) {
    int idx = slot & ~kFloatFlag;
    return idx | (isFloat ? kFloatFlag : 0);
}

static bool exprIsFloat(const Expr* e, const std::unordered_map<std::string, int>& locals) {
    if (dynamic_cast<const EFloat*>(e)) return true;
    if (dynamic_cast<const EInt*>(e)) return false;

    if (auto v = dynamic_cast<const EVar*>(e)) {
        auto it = locals.find(v->name);
        if (it == locals.end()) return false;
        return slotIsFloat(it->second);
    }

    if (auto c = dynamic_cast<const ECall*>(e)) {
        if (c->callee == "sqrt") return true;
        return false;
    }

    if (auto b = dynamic_cast<const EBin*>(e)) {
        switch (b->op) {
            case EBin::Le:
            case EBin::Lt:
            case EBin::Ge:
            case EBin::Gt:
            case EBin::Eq:
            case EBin::Ne:
            case EBin::Mod:
                return false;
            default:
                return exprIsFloat(b->a.get(), locals) || exprIsFloat(b->b.get(), locals);
        }
    }

    return false;
}

static int ensureLocal(std::unordered_map<std::string, int>& locals,
                       uint32_t& nextLocal,
                       const std::string& name) {
    auto it = locals.find(name);
    if (it != locals.end()) return it->second;

    int id = static_cast<int>(nextLocal++);
    locals[name] = id;
    return id;
}

void EInt::gen(Program& p, uint32_t, std::unordered_map<std::string, int>&, uint32_t&) {
    p.code.op(Op::ICONST);
    p.code.i64(v);
}

void EFloat::gen(Program& p, uint32_t, std::unordered_map<std::string, int>&, uint32_t&) {
    p.code.op(Op::FCONST);
    p.code.i64(bits);
}

void EVar::gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t&) {
    auto it = locals.find(name);
    if (it == locals.end()) throw std::runtime_error("unknown variable: " + name);

    p.code.op(Op::LOAD);
    p.code.u32(slotIndex(it->second));
}

void EBin::gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) {
    bool fa = exprIsFloat(a.get(), locals);
    bool fb = exprIsFloat(b.get(), locals);

    a->gen(p, 0, locals, nextLocal);
    b->gen(p, 0, locals, nextLocal);

    switch (op) {
        case Add: p.code.op((fa || fb) ? Op::FADD : Op::IADD);  break;
        case Sub: p.code.op((fa || fb) ? Op::FSUB : Op::ISUB);  break;
        case Mul: p.code.op((fa || fb) ? Op::FMUL : Op::IMUL);  break;
        case Div: p.code.op((fa || fb) ? Op::FDIV : Op::IDIV);  break;
        case Mod: p.code.op(Op::IMOD);  break;

        case Le:  p.code.op((fa || fb) ? Op::FCMPLE : Op::CMPLE); break;
        case Lt:  p.code.op((fa || fb) ? Op::FCMPLT : Op::CMPLT); break;
        case Ge:  p.code.op((fa || fb) ? Op::FCMPGE : Op::CMPGE); break;
        case Gt:  p.code.op((fa || fb) ? Op::FCMPGT : Op::CMPGT); break;
        case Eq:  p.code.op((fa || fb) ? Op::FCMPEQ : Op::CMPEQ); break;
        case Ne:  p.code.op((fa || fb) ? Op::FCMPNE : Op::CMPNE); break;
    }
}

void ECall::gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) {
    if (callee == "print") {
        if (args.size() != 1) throw std::runtime_error("print expects 1 arg");
        bool isF = exprIsFloat(args[0].get(), locals);
        args[0]->gen(p, 0, locals, nextLocal);
        p.code.op(isF ? Op::PRINT_F : Op::PRINT);
        p.code.op(Op::ICONST);
        p.code.i64(0);
        return;
    }
    if (callee == "print_big") {
        if (args.size() != 2) throw std::runtime_error("print_big expects 2 args");
        args[0]->gen(p, 0, locals, nextLocal);
        args[1]->gen(p, 0, locals, nextLocal);
        p.code.op(Op::PRINT_BIG);
        p.code.op(Op::ICONST);
        p.code.i64(0);
        return;
    }

    if (callee == "len") {
        if (args.size() != 1) throw std::runtime_error("len expects 1 arg");
        args[0]->gen(p, 0, locals, nextLocal);
        p.code.op(Op::ARRAY_LEN);
        return;
    }

    if (callee == "array") {
        if (args.size() != 1) throw std::runtime_error("array expects 1 arg");
        args[0]->gen(p, 0, locals, nextLocal);
        p.code.op(Op::ARRAY_NEW);
        return;
    }

    if (callee == "time_ms" || callee == "now") {
        if (!args.empty()) throw std::runtime_error("time_ms expects 0 args");
        p.code.op(Op::TIME_MS);
        return;
    }

    if (callee == "rand") {
        if (!args.empty()) throw std::runtime_error("rand expects 0 args");
        p.code.op(Op::RAND);
        return;
    }

    if (callee == "sqrt") {
        if (args.size() != 1) throw std::runtime_error("sqrt expects 1 arg");
        args[0]->gen(p, 0, locals, nextLocal);
        p.code.op(Op::FSQRT);
        return;
    }

    int fid = p.findFuncId(callee);
    if (fid < 0) throw std::runtime_error("unknown function: " + callee);

    const Function& F = p.funcs[static_cast<uint32_t>(fid)];
    if (args.size() != F.arity) {
        throw std::runtime_error(
                "function '" + callee + "' expects " + std::to_string(F.arity) +
                " args, got " + std::to_string(args.size())
        );
    }

    for (auto& a : args) a->gen(p, 0, locals, nextLocal);

    p.code.op(Op::CALL);
    p.code.u32(static_cast<uint32_t>(fid));
    p.code.u32(static_cast<uint32_t>(args.size()));
}

void EArrayIndex::gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) {
    array->gen(p, 0, locals, nextLocal);
    index->gen(p, 0, locals, nextLocal);
    p.code.op(Op::ARRAY_GET);
}

void SBlock::gen(Program& p, uint32_t currentFuncId, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) {
    for (auto& s : items) s->gen(p, currentFuncId, locals, nextLocal);
}

void SLet::gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) {
    int slot = ensureLocal(locals, nextLocal, name);

    if (init) {
        bool isF = exprIsFloat(init.get(), locals);
        locals[name] = slotWithFloat(slot, isF);

        init->gen(p, 0, locals, nextLocal);
        p.code.op(Op::STORE);
        p.code.u32(slotIndex(locals[name]));
    } else {
        locals[name] = slotWithFloat(slot, false);

        p.code.op(Op::ICONST);
        p.code.i64(0);
        p.code.op(Op::STORE);
        p.code.u32(slotIndex(locals[name]));
    }
}

void SAssign::gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) {
    (void)nextLocal;

    auto it = locals.find(name);
    if (it == locals.end()) throw std::runtime_error("assign to unknown var: " + name);

    bool isF = exprIsFloat(rhs.get(), locals);
    it->second = slotWithFloat(it->second, isF);

    rhs->gen(p, 0, locals, nextLocal);
    p.code.op(Op::STORE);
    p.code.u32(slotIndex(it->second));
}

void SArrayAssign::gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) {
    array->gen(p, 0, locals, nextLocal);
    index->gen(p, 0, locals, nextLocal);
    value->gen(p, 0, locals, nextLocal);
    p.code.op(Op::ARRAY_SET);
}

void SIf::gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) {
    cond->gen(p, 0, locals, nextLocal);

    p.code.op(Op::JMP_IF_FALSE);
    size_t jz = p.code.pc();
    p.code.u32(0);

    thenBlk->gen(p, 0, locals, nextLocal);

    if (elseBlk) {
        p.code.op(Op::JMP);
        size_t jend = p.code.pc();
        p.code.u32(0);

        size_t else_addr = p.code.pc();
        p.code.patch32(jz, static_cast<uint32_t>(else_addr));

        elseBlk->gen(p, 0, locals, nextLocal);

        size_t end_addr = p.code.pc();
        p.code.patch32(jend, static_cast<uint32_t>(end_addr));
    } else {
        size_t end_addr = p.code.pc();
        p.code.patch32(jz, static_cast<uint32_t>(end_addr));
    }
}

void SWhile::gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) {
    p.loopStack.emplace_back();

    size_t loop_start = p.code.pc();

    cond->gen(p, 0, locals, nextLocal);
    p.code.op(Op::JMP_IF_FALSE);
    size_t jz = p.code.pc();
    p.code.u32(0);

    body->gen(p, 0, locals, nextLocal);

    size_t continue_target = p.code.pc();
    for (size_t pos : p.loopStack.back().continuePatches) {
        p.code.patch32(pos, static_cast<uint32_t>(continue_target));
    }

    p.code.op(Op::JMP);
    p.code.u32(static_cast<uint32_t>(loop_start));

    size_t loop_end = p.code.pc();
    p.code.patch32(jz, static_cast<uint32_t>(loop_end));

    for (size_t pos : p.loopStack.back().breakPatches) {
        p.code.patch32(pos, static_cast<uint32_t>(loop_end));
    }

    p.loopStack.pop_back();
}

void SFor::gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) {
    p.loopStack.emplace_back();

    if (init) init->gen(p, 0, locals, nextLocal);

    size_t loop_start = p.code.pc();

    if (cond) {
        cond->gen(p, 0, locals, nextLocal);
    } else {
        p.code.op(Op::ICONST);
        p.code.i64(1);
    }

    p.code.op(Op::JMP_IF_FALSE);
    size_t jz = p.code.pc();
    p.code.u32(0);

    body->gen(p, 0, locals, nextLocal);

    size_t continue_target = p.code.pc();
    for (size_t pos : p.loopStack.back().continuePatches) {
        p.code.patch32(pos, static_cast<uint32_t>(continue_target));
    }

    if (step) step->gen(p, 0, locals, nextLocal);

    p.code.op(Op::JMP);
    p.code.u32(static_cast<uint32_t>(loop_start));

    size_t loop_end = p.code.pc();
    p.code.patch32(jz, static_cast<uint32_t>(loop_end));

    for (size_t pos : p.loopStack.back().breakPatches) {
        p.code.patch32(pos, static_cast<uint32_t>(loop_end));
    }

    p.loopStack.pop_back();
}

void SReturn::gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) {
    val->gen(p, 0, locals, nextLocal);
    p.code.op(Op::RET);
}

void SBreak::gen(Program& p, uint32_t, std::unordered_map<std::string, int>&, uint32_t&) {
    if (p.loopStack.empty()) {
        throw std::runtime_error("break outside of loop");
    }

    p.code.op(Op::JMP);
    size_t patch_pos = p.code.pc();
    p.code.u32(0);
    p.loopStack.back().breakPatches.emplace_back(patch_pos);
}

void SContinue::gen(Program& p, uint32_t, std::unordered_map<std::string, int>&, uint32_t&) {
    if (p.loopStack.empty()) {
        throw std::runtime_error("continue outside of loop");
    }

    p.code.op(Op::JMP);
    size_t patch_pos = p.code.pc();
    p.code.u32(0);
    p.loopStack.back().continuePatches.emplace_back(patch_pos);
}

void SExpr::gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) {
    e->gen(p, 0, locals, nextLocal);
    p.code.op(Op::POP);
}

void Module::gen(Program& p) {
    for (auto& f : funcs) {
        uint32_t arity = static_cast<uint32_t>(f->params.size());
        p.addFunc(f->name, arity, arity, 0);
    }

    for (auto& f : funcs) {
        auto fid = static_cast<uint32_t>(p.findFuncId(f->name));
        auto& F = p.funcs[fid];
        F.entry = p.code.pc();

        std::unordered_map<std::string, int> locals;
        for (size_t i = 0; i < f->params.size(); ++i) {
            locals[f->params[i]] = static_cast<int>(i);
        }
        uint32_t nextLocal = static_cast<uint32_t>(f->params.size());

        f->body->gen(p, fid, locals, nextLocal);

        p.code.op(Op::ICONST);
        p.code.i64(0);
        p.code.op(Op::RET);

        F.nlocals = nextLocal;
        F.end = p.code.pc();
        F.maxStack = computeMaxStack(p, F);
    }
}
