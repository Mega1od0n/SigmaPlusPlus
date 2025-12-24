#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "vm.h"

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool startsWith(const std::string& s, const std::string& pref) {
    return s.rfind(pref, 0) == 0;
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            return 2;
        }

        std::string file = argv[1];
        bool enableJit = true;
        size_t gcTh = 100;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--no-jit") {
                enableJit = false;
            } else if (startsWith(arg, "--gc=")) {
                gcTh = static_cast<size_t>(std::stoull(arg.substr(5)));
            } else {
                std::cerr << "Unknown arg: " << arg << "\n";
                return 2;
            }
        }

        std::string src = readFile(file);
        Lexer lx(std::move(src));
        auto toks = lx.lex();
        Parser ps(std::move(toks));
        auto mod = ps.parseModule();

        Program prog;
        mod->gen(prog);

        VM vm(&prog);
        vm.gcThreshold = gcTh;

        if (!enableJit) {
            vm.jit.reset();
        }

        vm.run("main");
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
