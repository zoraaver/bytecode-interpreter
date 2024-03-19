#include "compiler.h"

#include <print>
#include <string_view>

#include "scanner.h"

namespace lox
{

Compiler::Compiler() { }

std::expected<Chunk, CompilerError> Compiler::compile()
{
    Chunk chunk;

    int line = -1;
    // while(true)
    // {
    // _parser.advance();
    // }

    _emit_return(chunk);

    // if(_parser.has_error())
    //     return std::unexpected(CompilerError::ParserError);

    return chunk;
}

void Compiler::_emit_bytes(Chunk& chunk, uint8_t byte)
{
    // chunk.write(byte, _parser.get_previous_line());
}

} // namespace lox