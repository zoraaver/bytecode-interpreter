#include <fstream>
#include <iostream>
#include <print>
#include <sstream>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "object.h"
#include "parser.h"
#include "scanner.h"
#include "stack.h"
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
        // vm.interpret();
    }
}

void run_file(std::string_view filename)
{
    const auto source = read_file(filename);

    lox::Scanner scanner{source};

    lox::CallStack callstack;
    lox::FixedStack<lox::Value> stack;
    lox::HashMap<lox::Value> globals;
    std::vector<lox::UpValueObject*> open_upvalues;

    lox::ObjectAllocator allocator{stack, globals, callstack, open_upvalues};

    lox::Parser parser{scanner, allocator};

    auto declarations = parser.parse();

    if(!declarations)
    {
        std::exit(65);
    }

    lox::Compiler compiler{allocator};

    auto script = compiler.compile(declarations.value());

    if(!script)
    {
        std::exit(70);
    }

    lox::VM vm{allocator, stack, globals, callstack, open_upvalues};
    auto result = vm.interpret(*script.value());

    if(result == lox::InterpretResult::RUNTIME_ERROR)
        std::exit(70);
}
} // namespace

int main(int argc, const char* argv[])
{
    lox::Chunk chunk;
    if(argc == 1)
    {
        // repl(vm);
    }
    else if(argc == 2)
    {
        run_file(argv[1]);
    }
    else
    {
        std::println(stderr, "Usage: clox [path]\n");
        std::exit(64);
    }
}