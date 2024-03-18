#include "chunk.h"

#include <cstdint>
#include <print>
#include <string_view>

namespace lox
{
namespace
{

int simple_instruction(std::string_view name, int offset)
{
    std::println("{}", name);
    return offset + 1;
}

} // namespace

void Chunk::disassemble(std::string_view name) const
{
    std::println("== {} ==", name);

    for(size_t offset = 0; offset < _code.size();)
    {
        offset = _disassemble_instruction(offset);
    }
}

void Chunk::write(OpCode byte)
{
    _code.push_back(static_cast<uint8_t>(byte));
}

int Chunk::_disassemble_instruction(int offset) const
{
    std::print("{:04d} ", offset);

    auto instruction = static_cast<OpCode>(_code.at(offset));

    switch(instruction)
    {
    case OpCode::RETURN:
        return simple_instruction("RETURN", offset);
    default:
        std::println("Unknown instruction {}", static_cast<uint8_t>(instruction));
        return offset + 1;
    }
}

} // namespace lox