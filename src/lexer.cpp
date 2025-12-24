#include "lexer.h"
#include <cctype>
#include <cstring>
#include <stdexcept>

char Lexer::get() {
    if (eof()) return '\0';

    char c = src[i++];
    if (c == '\n') {
        line++;
        col = 1;
    } else {
        col++;
    }
    return c;
}

void Lexer::skipSpaceAndComments() {
    for (;;) {
        while (std::isspace((unsigned char)peek())) get();

        if (peek() == '/' && peek(1) == '/') {
            while (!eof() && peek() != '\n') get();
            continue;
        }

        break;
    }
}

Token Lexer::identOrKeyword() {
    Token t{TokKind::Ident, "", 0, line, col};
    while (std::isalnum((unsigned char)peek()) || peek() == '_') t.text.push_back(get());

    if (t.text == "fn") t.kind = TokKind::KwFn;
    else if (t.text == "return") t.kind = TokKind::KwReturn;
    else if (t.text == "if") t.kind = TokKind::KwIf;
    else if (t.text == "else") t.kind = TokKind::KwElse;
    else if (t.text == "let") t.kind = TokKind::KwLet;
    else if (t.text == "while") t.kind = TokKind::KwWhile;
    else if (t.text == "for") t.kind = TokKind::KwFor;
    else if (t.text == "break") t.kind = TokKind::KwBreak;
    else if (t.text == "continue") t.kind = TokKind::KwContinue;

    return t;
}

static int64_t doubleToI64(double d) {
    int64_t bits = 0;
    std::memcpy(&bits, &d, sizeof(double));
    return bits;
}

Token Lexer::number() {
    Token t{TokKind::Int, "", 0, line, col};

    bool isFloat = false;

    if (peek() == '.') {
        isFloat = true;
        t.text.push_back('0');
        t.text.push_back(get());
        while (std::isdigit((unsigned char)peek())) t.text.push_back(get());
    } else {
        while (std::isdigit((unsigned char)peek())) t.text.push_back(get());
        if (peek() == '.') {
            isFloat = true;
            t.text.push_back(get());
            while (std::isdigit((unsigned char)peek())) t.text.push_back(get());
        }
    }

    if (peek() == 'e' || peek() == 'E') {
        isFloat = true;
        t.text.push_back(get());
        if (peek() == '+' || peek() == '-') t.text.push_back(get());
        if (!std::isdigit((unsigned char)peek())) {
            throw std::runtime_error("bad float exponent");
        }
        while (std::isdigit((unsigned char)peek())) t.text.push_back(get());
    }

    if (isFloat) {
        t.kind = TokKind::Float;
        double d = std::stod(t.text);
        t.ival = doubleToI64(d);
    } else {
        t.kind = TokKind::Int;
        t.ival = std::stoll(t.text);
    }

    return t;
}

std::vector<Token> Lexer::lex() {
    std::vector<Token> out;

    skipSpaceAndComments();
    while (!eof()) {
        char c = peek();

        if (std::isalpha((unsigned char)c) || c == '_') {
            out.emplace_back(identOrKeyword());
        } else if (std::isdigit((unsigned char)c) || (c == '.' && std::isdigit((unsigned char)peek(1)))) {
            out.emplace_back(number());
        } else {
            Token t{TokKind::Unknown, std::string(1, c), 0, line, col};

            switch (c) {
                case '(': t.kind = TokKind::LParen; get(); break;
                case ')': t.kind = TokKind::RParen; get(); break;
                case '{': t.kind = TokKind::LBrace; get(); break;
                case '}': t.kind = TokKind::RBrace; get(); break;
                case '[': t.kind = TokKind::LBracket; get(); break;
                case ']': t.kind = TokKind::RBracket; get(); break;
                case ',': t.kind = TokKind::Comma; get(); break;
                case ';': t.kind = TokKind::Semicolon; get(); break;
                case '+': t.kind = TokKind::Plus; get(); break;

                case '-':
                    if (peek(1) == '>') {
                        get();
                        get();
                        t.kind = TokKind::Arrow;
                        t.text = "->";
                    } else {
                        get();
                        t.kind = TokKind::Minus;
                    }
                    break;

                case '*': t.kind = TokKind::Star; get(); break;
                case '/': t.kind = TokKind::Slash; get(); break;
                case '%': t.kind = TokKind::Percent; get(); break;

                case '=':
                    if (peek(1) == '=') {
                        get();
                        get();
                        t.kind = TokKind::Eq;
                        t.text = "==";
                    } else {
                        get();
                        t.kind = TokKind::Assign;
                    }
                    break;

                case '<':
                    if (peek(1) == '=') {
                        get();
                        get();
                        t.kind = TokKind::Le;
                        t.text = "<=";
                    } else {
                        get();
                        t.kind = TokKind::Lt;
                        t.text = "<";
                    }
                    break;

                case '>':
                    if (peek(1) == '=') {
                        get();
                        get();
                        t.kind = TokKind::Ge;
                        t.text = ">=";
                    } else {
                        get();
                        t.kind = TokKind::Gt;
                        t.text = ">";
                    }
                    break;

                case '!':
                    if (peek(1) == '=') {
                        get();
                        get();
                        t.kind = TokKind::Ne;
                        t.text = "!=";
                    } else {
                        get();
                    }
                    break;

                default:
                    get();
                    break;
            }

            if (t.kind != TokKind::Unknown) out.emplace_back(t);
        }

        skipSpaceAndComments();
    }

    out.emplace_back(Token{TokKind::End, "", 0, line, col});
    return out;
}
