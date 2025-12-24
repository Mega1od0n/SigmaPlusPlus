#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "bytecode.h"

struct Expr;
struct Stmt;
struct Func;
struct Module;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct Expr {
    virtual ~Expr() = default;
    virtual void gen(Program& p, uint32_t currentFuncId, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) = 0;
};

struct EInt : Expr {
    int64_t v;
    explicit EInt(int64_t v) : v(v) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>&, uint32_t&) override;
};

struct EFloat : Expr {
    int64_t bits;
    explicit EFloat(int64_t bits) : bits(bits) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>&, uint32_t&) override;
};

struct EVar : Expr {
    std::string name;
    explicit EVar(std::string n) : name(std::move(n)) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t&) override;
};

struct EBin : Expr {
    enum Op2 { Add, Sub, Mul, Div, Mod, Le, Lt, Ge, Gt, Eq, Ne } op;
    ExprPtr a, b;
    EBin(Op2 op, ExprPtr a, ExprPtr b) : op(op), a(std::move(a)), b(std::move(b)) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>&, uint32_t&) override;
};

struct ECall : Expr {
    std::string callee;
    std::vector<ExprPtr> args;
    ECall(std::string c, std::vector<ExprPtr> a) : callee(std::move(c)), args(std::move(a)) {}
    void gen(Program& p, uint32_t currentFuncId, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) override;
};

struct EArrayIndex : Expr {
    ExprPtr array;
    ExprPtr index;
    EArrayIndex(ExprPtr a, ExprPtr i) : array(std::move(a)), index(std::move(i)) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) override;
};

struct Stmt {
    virtual ~Stmt() = default;
    virtual void gen(Program& p, uint32_t currentFuncId, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) = 0;
};

struct SBlock : Stmt {
    std::vector<StmtPtr> items;
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>&, uint32_t&) override;
};

struct SLet : Stmt {
    std::string name;
    ExprPtr init;
    explicit SLet(std::string n, ExprPtr i) : name(std::move(n)), init(std::move(i)) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) override;
};

struct SAssign : Stmt {
    std::string name;
    ExprPtr rhs;
    SAssign(std::string n, ExprPtr r) : name(std::move(n)), rhs(std::move(r)) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) override;
};

struct SArrayAssign : Stmt {
    ExprPtr array;
    ExprPtr index;
    ExprPtr value;
    SArrayAssign(ExprPtr a, ExprPtr i, ExprPtr v) : array(std::move(a)), index(std::move(i)), value(std::move(v)) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) override;
};

struct SIf : Stmt {
    ExprPtr cond;
    std::unique_ptr<SBlock> thenBlk;
    std::unique_ptr<SBlock> elseBlk;
    SIf(ExprPtr c, std::unique_ptr<SBlock> t, std::unique_ptr<SBlock> e) : cond(std::move(c)), thenBlk(std::move(t)), elseBlk(std::move(e)) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) override;
};

struct SWhile : Stmt {
    ExprPtr cond;
    std::unique_ptr<SBlock> body;
    SWhile(ExprPtr c, std::unique_ptr<SBlock> b) : cond(std::move(c)), body(std::move(b)) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) override;
};

struct SFor : Stmt {
    StmtPtr init;
    ExprPtr cond;
    StmtPtr step;
    std::unique_ptr<SBlock> body;
    SFor(StmtPtr i, ExprPtr c, StmtPtr s, std::unique_ptr<SBlock> b) : init(std::move(i)), cond(std::move(c)), step(std::move(s)), body(std::move(b)) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) override;
};

struct SReturn : Stmt {
    ExprPtr val;
    explicit SReturn(ExprPtr v) : val(std::move(v)) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) override;
};

struct SBreak : Stmt {
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) override;
};

struct SContinue : Stmt {
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) override;
};

struct SExpr : Stmt {
    ExprPtr e;
    explicit SExpr(ExprPtr e) : e(std::move(e)) {}
    void gen(Program& p, uint32_t, std::unordered_map<std::string, int>& locals, uint32_t& nextLocal) override;
};

struct Func {
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<SBlock> body;
};

struct Module {
    std::vector<std::unique_ptr<Func>> funcs;
    void gen(Program& p);
};
