#include <fstream>
#include <iostream>
#include <print>
#include <sstream>

#include "chunk.h"
#include "compiler.h"
#include "parser.h"
#include "scanner.h"
#include "vm.h"

namespace
{
std::string read_file(std::string_view filename)
{
    std::ifstream ifs(filename);

    if(ifs.fail())
    {
        throw std::runtime_error{"Failed to read file"};
    }

    std::stringstream buf;

    buf << ifs.rdbuf();

    return buf.str();
}

void repl(lox::VM& vm)
{
    std::print("> ");
    for(std::string line; std::getline(std::cin, line);)
    {
        std::print("> ");
        vm.interpret(line);
    }
}

void run_file(std::string_view filename, lox::VM& vm)
{
    const auto source = read_file(filename);
    lox::Scanner scanner{source};
    lox::Parser parser{scanner};

    auto expr = parser.parse();

    if(!expr)
    {
        std::exit(65);
    }

    lox::Compiler compiler;

    compiler.compile();

    auto result = vm.interpret(source);

    if(result == lox::InterpretResult::COMPILE_ERROR)
        std::exit(65);

    if(result == lox::InterpretResult::RUNTIME_ERROR)
        std::exit(70);
}
} // namespace

int main(int argc, const char* argv[])
{
    lox::Chunk chunk;
    lox::VM vm{chunk};
    if(argc == 1)
    {
        repl(vm);
    }
    else if(argc == 2)
    {
        run_file(argv[1], vm);
    }
    else
    {
        std::println(stderr, "Usage: clox [path]\n");
        std::exit(64);
    }
}