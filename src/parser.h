#pragma once

#include "lexer.h"
#include "ast.h"
#include <memory>

struct Parser {
    explicit Parser(std::vector<Token>&& toks) : ts(std::move(toks)) {}

    std::unique_ptr<Module> parseModule();

private:
    const std::vector<Token>& ts;
    size_t i = 0;

    const Token& cur() const { return ts[i]; }
    bool accept(TokKind k);
    void expect(TokKind k, const char* msg);

    std::string error(const std::string& msg) const { return msg + " at line " + std::to_string(cur().line) + ", col " + std::to_string(cur().col) + " (token: '" + cur().text + "')"; }

    std::unique_ptr<Func> parseFunc();
    std::unique_ptr<SBlock> parseBlock();
    StmtPtr parseStmt();
    ExprPtr parseExpr();
    ExprPtr parsePrimary();
    ExprPtr parseBinRhs(int minPrec, ExprPtr lhs);
    int precOf(TokKind k);
};
