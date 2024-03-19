#ifndef LOX_COMPILER_H
#define LOX_COMPILER_H

#include <cstdint>
#include <expected>
#include <string_view>

#include "chunk.h"

namespace lox
{

enum class CompilerError
{
    ParserError
};

class Compiler
{
    void _emit_bytes(Chunk& chunk, uint8_t byte);

    void _emit_bytecode(Chunk& chunk, OpCode code)
    {
        _emit_bytes(chunk, static_cast<uint8_t>(code));
    };

    void _emit_return(Chunk& chunk)
    {
        _emit_bytecode(chunk, OpCode::RETURN);
    }

    template <typename... Bytes>
    void _emit_bytes(Chunk& chunk, uint8_t byte, Bytes... bytes)
    {
        _emit_bytes(chunk, byte);
        _emit_bytes(chunk, bytes...);
    }

public:
    Compiler();
    std::expected<Chunk, CompilerError> compile();
};
} // namespace lox

#endif // LOX_COMPILER_H