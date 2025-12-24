#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class TokKind {
    End,
    Ident,
    Int,
    Float,
    KwFn, KwReturn, KwIf, KwElse, KwLet, KwWhile, KwFor, KwBreak, KwContinue,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Semicolon, Arrow,
    Plus, Minus, Star, Slash, Percent,
    Assign,
    Le,
    Lt,
    Ge,
    Gt,
    Eq,
    Ne,
    Unknown
};

struct Token {
    TokKind kind;
    std::string text;
    int64_t ival = 0;
    int line = 1;
    int col = 1;
};

struct Lexer {
    explicit Lexer(std::string&& s) : src(std::move(s)) {}
    std::vector<Token> lex();

private:
    const std::string src;
    size_t i = 0;
    int line = 1, col = 1;
    bool eof() const { return i >= src.size(); }
    char peek(int k=0) const { return (i+k < src.size()) ? src[i+k] : '\0'; }
    char get();
    void skipSpaceAndComments();
    Token identOrKeyword();
    Token number();
};
