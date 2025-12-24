#include "parser.h"
#include <stdexcept>

bool Parser::accept(TokKind k) {
    if (cur().kind == k) { ++i; return true; }
    return false;
}

void Parser::expect(TokKind k, const char* msg) {
    if (!accept(k)) throw std::runtime_error(error(std::string("expected ") + msg));
}

int Parser::precOf(TokKind k) {
    switch (k) {
        case TokKind::Eq:
        case TokKind::Ne: return 4;

        case TokKind::Le:
        case TokKind::Lt:
        case TokKind::Ge:
        case TokKind::Gt: return 5;

        case TokKind::Plus:
        case TokKind::Minus: return 10;

        case TokKind::Star:
        case TokKind::Slash:
        case TokKind::Percent: return 20;

        default: return -1;
    }
}

std::unique_ptr<SBlock> Parser::parseBlock() {
    expect(TokKind::LBrace, "'{'");
    auto blk = std::make_unique<SBlock>();

    while (cur().kind != TokKind::RBrace) {
        blk->items.emplace_back(parseStmt());
    }

    expect(TokKind::RBrace, "'}'");
    return blk;
}

StmtPtr Parser::parseStmt() {
    if (accept(TokKind::KwLet)) {
        if (cur().kind != TokKind::Ident) throw std::runtime_error(error("expected identifier after let"));
        std::string name = cur().text; ++i;

        ExprPtr init;
        if (accept(TokKind::Assign)) init = parseExpr();

        expect(TokKind::Semicolon, "';'");
        return std::make_unique<SLet>(name, std::move(init));
    }

    if (accept(TokKind::KwReturn)) {
        auto e = parseExpr();
        expect(TokKind::Semicolon, "';'");
        return std::make_unique<SReturn>(std::move(e));
    }

    if (accept(TokKind::KwBreak)) {
        expect(TokKind::Semicolon, "';'");
        return std::make_unique<SBreak>();
    }

    if (accept(TokKind::KwContinue)) {
        expect(TokKind::Semicolon, "';'");
        return std::make_unique<SContinue>();
    }

    if (accept(TokKind::KwIf)) {
        expect(TokKind::LParen, "'('");
        auto cond = parseExpr();
        expect(TokKind::RParen, "')'");
        auto thenBlk = parseBlock();

        std::unique_ptr<SBlock> elseBlk;
        if (accept(TokKind::KwElse)) elseBlk = parseBlock();

        return std::make_unique<SIf>(std::move(cond), std::move(thenBlk), std::move(elseBlk));
    }

    if (accept(TokKind::KwWhile)) {
        expect(TokKind::LParen, "'('");
        auto cond = parseExpr();
        expect(TokKind::RParen, "')'");
        auto body = parseBlock();
        return std::make_unique<SWhile>(std::move(cond), std::move(body));
    }

    if (accept(TokKind::KwFor)) {
        expect(TokKind::LParen, "'('");

        StmtPtr init;
        if (cur().kind != TokKind::Semicolon) {
            if (cur().kind == TokKind::KwLet) {
                accept(TokKind::KwLet);
                std::string name = cur().text; ++i;

                ExprPtr initExpr;
                if (accept(TokKind::Assign)) initExpr = parseExpr();

                init = std::make_unique<SLet>(name, std::move(initExpr));
            } else {
                std::string name = cur().text; ++i;
                expect(TokKind::Assign, "'='");
                auto e = parseExpr();
                init = std::make_unique<SAssign>(name, std::move(e));
            }
        }

        expect(TokKind::Semicolon, "';'");

        ExprPtr cond;
        if (cur().kind != TokKind::Semicolon) cond = parseExpr();
        expect(TokKind::Semicolon, "';'");

        StmtPtr step;
        if (cur().kind != TokKind::RParen) {
            std::string name = cur().text; ++i;
            expect(TokKind::Assign, "'='");
            auto e = parseExpr();
            step = std::make_unique<SAssign>(name, std::move(e));
        }

        expect(TokKind::RParen, "')'");
        auto body = parseBlock();
        return std::make_unique<SFor>(std::move(init), std::move(cond), std::move(step), std::move(body));
    }

    if (cur().kind == TokKind::Ident) {
        size_t save_pos = i;
        std::string name = cur().text; ++i;

        if (cur().kind == TokKind::Assign) {
            expect(TokKind::Assign, "'='");
            auto e = parseExpr();
            expect(TokKind::Semicolon, "';'");
            return std::make_unique<SAssign>(name, std::move(e));
        } else if (cur().kind == TokKind::LBracket) {
            expect(TokKind::LBracket, "'['");
            auto idx = parseExpr();
            expect(TokKind::RBracket, "']'");

            if (accept(TokKind::Assign)) {
                auto val = parseExpr();
                expect(TokKind::Semicolon, "';'");
                return std::make_unique<SArrayAssign>(std::make_unique<EVar>(name), std::move(idx), std::move(val));
            }

            i = save_pos;
            auto e = parseExpr();
            expect(TokKind::Semicolon, "';'");
            return std::make_unique<SExpr>(std::move(e));
        } else {
            i = save_pos;
        }
    }

    auto e = parseExpr();
    expect(TokKind::Semicolon, "';'");
    return std::make_unique<SExpr>(std::move(e));
}

ExprPtr Parser::parsePrimary() {
    if (cur().kind == TokKind::Minus) {
        ++i;
        auto zero = std::make_unique<EInt>(0);
        auto rhs = parsePrimary();
        return std::make_unique<EBin>(EBin::Sub, std::move(zero), std::move(rhs));
    }

    ExprPtr lhs;
    if (cur().kind == TokKind::Int) {
        lhs = std::make_unique<EInt>(cur().ival); ++i;
    } else if (cur().kind == TokKind::Float) {
        lhs = std::make_unique<EFloat>(cur().ival); ++i;
    } else if (cur().kind == TokKind::Ident) {
        std::string name = cur().text; ++i;

        if (accept(TokKind::LParen)) {
            std::vector<ExprPtr> args;

            if (cur().kind != TokKind::RParen) {
                args.emplace_back(parseExpr());
                while (accept(TokKind::Comma)) args.emplace_back(parseExpr());
            }

            expect(TokKind::RParen, "')'");
            lhs = std::make_unique<ECall>(name, std::move(args));
        } else {
            lhs = std::make_unique<EVar>(name);
        }
    } else if (accept(TokKind::LParen)) {
        lhs = parseExpr();
        expect(TokKind::RParen, "')'");
    } else {
        throw std::runtime_error(error("unexpected token in expression"));
    }

    while (accept(TokKind::LBracket)) {
        auto index = parseExpr();
        expect(TokKind::RBracket, "']'");
        lhs = std::make_unique<EArrayIndex>(std::move(lhs), std::move(index));
    }

    return lhs;
}

ExprPtr Parser::parseExpr() {
    auto lhs = parsePrimary();
    return parseBinRhs(0, std::move(lhs));
}

ExprPtr Parser::parseBinRhs(int minPrec, ExprPtr lhs) {
    for (;;) {
        int prec = precOf(cur().kind);
        if (prec < minPrec) return lhs;

        TokKind opTok = cur().kind; ++i;
        auto rhs = parsePrimary();

        int nextPrec = precOf(cur().kind);
        if (nextPrec > prec) rhs = parseBinRhs(prec + 1, std::move(rhs));

        EBin::Op2 op;
        switch (opTok) {
            case TokKind::Plus:    op = EBin::Add; break;
            case TokKind::Minus:   op = EBin::Sub; break;
            case TokKind::Star:    op = EBin::Mul; break;
            case TokKind::Slash:   op = EBin::Div; break;
            case TokKind::Percent: op = EBin::Mod; break;
            case TokKind::Le:      op = EBin::Le;  break;
            case TokKind::Lt:      op = EBin::Lt;  break;
            case TokKind::Ge:      op = EBin::Ge;  break;
            case TokKind::Gt:      op = EBin::Gt;  break;
            case TokKind::Eq:      op = EBin::Eq;  break;
            case TokKind::Ne:      op = EBin::Ne;  break;
            default: throw std::runtime_error(error("unknown binary operator"));
        }

        lhs = std::make_unique<EBin>(op, std::move(lhs), std::move(rhs));
    }
}

std::unique_ptr<Func> Parser::parseFunc() {
    expect(TokKind::KwFn, "'fn'");
    if (cur().kind != TokKind::Ident) throw std::runtime_error(error("expected function name"));

    auto name = cur().text; ++i;

    expect(TokKind::LParen, "'('");
    std::vector<std::string> params;

    if (cur().kind != TokKind::RParen) {
        if (cur().kind != TokKind::Ident) throw std::runtime_error(error("expected param name"));
        params.emplace_back(cur().text); ++i;

        while (accept(TokKind::Comma)) {
            if (cur().kind != TokKind::Ident) throw std::runtime_error(error("expected param name"));
            params.emplace_back(cur().text); ++i;
        }
    }

    expect(TokKind::RParen, "')'");

    if (accept(TokKind::Arrow)) {
        if (cur().kind == TokKind::Ident) ++i;
    }

    auto body = parseBlock();
    auto fn = std::make_unique<Func>();
    fn->name = std::move(name);
    fn->params = std::move(params);
    fn->body = std::move(body);
    return fn;
}

std::unique_ptr<Module> Parser::parseModule() {
    auto m = std::make_unique<Module>();
    while (cur().kind != TokKind::End) m->funcs.emplace_back(parseFunc());
    return m;
}
