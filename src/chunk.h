#ifndef LOX_CHUNK_H
#define LOX_CHUNK_H

#include <cstdint>
#include <string_view>
#include <vector>

#include "value.h"

namespace lox
{

enum class OpCode : uint8_t
{
    RETURN,
    CONSTANT
};

class Chunk
{
    std::vector<uint8_t> _code;
    std::vector<Value> _constants;
    std::vector<int> _lines;

    int _disassemble_instruction(int offset) const;
    int _constant_instruction(std::string_view name, int offset) const;

public:
    void disassemble(std::string_view name) const;
    void write(OpCode, int line);
    void write(uint8_t byte, int line);
    int add_constant(Value);
};

} // namespace lox

#endif // LOX_CHUNK_H