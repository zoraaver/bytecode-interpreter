#include "compiler.h"

#include <print>

#include "scanner.h"

namespace lox
{

Compiler::Compiler(Scanner& scanner)
    : _scanner(scanner){};

void Compiler::compile()
{
    int line = -1;
    while(true)
    {
        auto token = _scanner.scan_token();
        if(token.line != line)
        {
            std::print("{:4d} ", token.line);
            line = token.line;
        }
        else
        {
            std::print("   | ");
        }
        std::print(
            "{} '{:{}s}'\n", get_token_type_name(token.type), token.lexeme, token.lexeme.size());

        if(token.type == TokenType::END_OF_FILE)
            break;
    }
}

} // namespace lox