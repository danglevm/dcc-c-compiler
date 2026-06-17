#include <iostream>
#include <span>
#include <string_view>
#include "CLI/CLI11.hpp"
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
#include <string>

#include <libdcc/lexer.hpp>
#include <libdcc/parser.hpp>
#include <libdcc/asm.hpp>
#include <libdcc/tacky.hpp>

namespace fs = std::filesystem;

const int SUCCESS = 0;
const int FAILED = 1;


namespace {

    int run_command(std::vector<std::string>& args_str) {
        std::vector<char*> args;

        for (auto& s : args_str) {
            args.push_back(s.data());
        }
        args.push_back(nullptr); /* null-terminated */

        pid_t ret = fork();

        if (ret == -1) {
            std::cerr << "Cannot run command\n";
            return FAILED;
        } else if (ret == 0) {
            /* child process: run GCC */
            execvp(args[0], args.data());
            std::cerr << "Failed to execute gcc\n";
        } else {
            /* parent process: driver program */
            int status;
            waitpid(ret, &status, 0); //wait for GCC to finish

            if (WIFEXITED(status)) {
                return WEXITSTATUS(status); /* return GCC exit code */
            }
        }

        return FAILED;
    } 
}


int main(int argc, char * argv[]) {
    
    // std::span<char*> args(argv, argc);
    
    // auto compiler_args = args.subspan(1);

    // for (const auto& arg : compiler_args) { 
    //     std::string_view sv{arg};

    //     if (sv.ends_with(".c")) {

    //     } 
    // }

    CLI::App app{"C Compiler Driver"};
    argv = app.ensure_utf8(argv);

    std::string_view input_file;
    bool preprocess_only = false;
    bool assemble_only = false;
    bool lex = false;
    bool parse = false;
    bool codegen = false;
    bool tacky = false;

    app.add_option("input", input_file, "Path to .c file")
        ->required()
        ->check(CLI::ExistingFile);

    app.add_flag("-E", preprocess_only, "Stop after the preprocessing stage; do not run the compiler proper.");
    app.add_flag("-S", assemble_only, "Stop after the stage of compilation proper; do not assemble.");
    app.add_flag("--lex", lex, "Directs to run lexer but stops before parsing");
    app.add_flag("--parse", parse, "Directs to run parser but stops before asm generation");
    app.add_flag("--tacky", tacky, "Directs to run tacky generation");
    app.add_flag("--codegen", codegen, "Directs to run lexing, parsing, and asm generation but stops before parsing");

    CLI11_PARSE(app, argc, argv);

    fs::path input_path(input_file);
    if (input_path.extension() != ".c") {
        std::cerr << "Error: Input file must have a .c extension.\n";
        return FAILED;
    }

    fs::path pp_path = input_path;
    pp_path.replace_extension(".i");
    fs::path asm_path = input_path;
    asm_path.replace_extension(".s");
    fs::path executable_path = input_path;
    executable_path.replace_extension("");

    /* PHASE 1: PREPROCESSING (.c -> .i) */
    std::vector<std::string> pp_args = {
        "gcc","-E","-P", input_path.string(),"-o", pp_path.string()
    };

    if (int result = run_command(pp_args); result != SUCCESS) {
        std::cerr << "Error: Input file must have a .c extension.\n";
        return FAILED;
    } 


    /* PHASE 2: LEXING */
    std::ifstream file(pp_path, std::ios_base::in);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file with .i extension\n";
        return FAILED;
    }

    /* early exit if preprocessing set for -E */
    if (preprocess_only) {
        return SUCCESS;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    dcc::token token;
    bool lex_error = false;
    dcc::Lexer lexer(source);
    std::vector<dcc::token> tokens;

    do {
        token = lexer.get_next_token();
        if (token.token == dcc::token_type::INVALID) {
            std::cerr << "Lexer error at line " << token.line << ", column " << token.column << "\n";
            lex_error = true;
            break;
        }
        
        std::cout << "Token: " << token.text << "\n";
        tokens.emplace_back(token);

    } while (token.token != dcc::token_type::ENDOFFILE);

    if (lex_error) {
        if (fs::exists((pp_path))) {
            fs::remove(pp_path);
        }
        return FAILED;
    }

    if (lex) {
        if (fs::exists((pp_path))) {
            fs::remove(pp_path);
        }
        return SUCCESS;
    }

    /* PHASE 3: PARSING */
    dcc::Parser parser(tokens);
    
    std::unique_ptr<dcc::Program> ast = parser.parse_program();
    ast->print();

    if (parse) {
        if (fs::exists((pp_path))) fs::remove(pp_path);
        return SUCCESS;
    }

    /* PHASE 4: CODE GENERATION (.i -> .s)*/
    assembly::CodeGen cg;
    tacky::TackyGen tg;
    /* 4.1. generate TACKY IR from AST */
    std::unique_ptr<tacky::TackyProgram> tacky_ir = tg.generate_tacky(ast.get());

    if (tacky) {
        if (fs::exists((pp_path))) fs::remove(pp_path);
        return SUCCESS;
    }

    /* 4.2. generate Assembly IR from TACKY IR */
    std::unique_ptr<assembly::IRProgram> ir = cg.generate(tacky_ir.get());
    /* code generation and emission */
    if (codegen) {
        if (fs::exists(pp_path)) fs::remove(pp_path);
        return SUCCESS;
    }
    std::string assembly = ir->code_gen();

    // fs::path("/home/dangle/dcc/build/test/writing-a-c-compiler-tests/tests/chapter_1/valid/");
    std::ofstream asm_file(asm_path);
    if (!asm_file.is_open()) {
        return FAILED;
    }
    
    asm_file << assembly;
    asm_file << ".section .note.GNU-stack,\"\",@progbits\n";
    asm_file.close();
    
    // -S 
    if (assemble_only) { 
        if (fs::exists(pp_path)) fs::remove(pp_path);
        return SUCCESS;
    }

    /* PHASE 5: LINKING (.s->executable)*/
    std::vector<std::string> link_args = {
        "gcc", asm_path.string(), "-o", executable_path.string()
    };

    int link_result = run_command(link_args);
    if (fs::exists(pp_path)) fs::remove(pp_path);
    if (fs::exists(asm_path)) fs::remove(asm_path);

    return link_result == 0 ? SUCCESS : FAILED;

    return SUCCESS;
}


