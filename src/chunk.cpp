#include "chunk.h"

#include <cassert>
#include <cstdint>
#include <print>
#include <stdexcept>
#include <string_view>

#include "value.h"

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

int Chunk::_constant_instruction(std::string_view name, int offset) const
{
    auto constant = _code.at(offset + 1);
    std::print("{:16} {:4} ", name, constant);
    print_value(_constants.at(constant));
    std::println("");

    return offset + 2;
}

void Chunk::disassemble(std::string_view name) const
{
    std::println("== {} ==", name);

    for(size_t offset = 0; offset < _code.size();)
    {
        offset = _disassemble_instruction(offset);
    }
}

void Chunk::write(OpCode byte, int line)
{
    write(static_cast<uint8_t>(byte), line);
}

void Chunk::write(uint8_t byte, int line)
{
    _code.push_back(byte);
    _lines.push_back(line);
}

int Chunk::_disassemble_instruction(int offset) const
{
    std::print("{:04d} ", offset);
    if(offset > 0 && _lines.at(offset) == _lines[offset - 1])
    {
        std::print("   | ");
    }
    else
    {
        std::print("{:4d} ", _lines.at(offset));
    }

    auto instruction = static_cast<OpCode>(_code.at(offset));

    switch(instruction)
    {
    case OpCode::RETURN:
        return simple_instruction("RETURN", offset);
    case OpCode::CONSTANT:
        return _constant_instruction("CONSTANT", offset);
    case OpCode::NEGATE:
        return simple_instruction("NEGATE", offset);
    case OpCode::ADD:
        return simple_instruction("ADD", offset);
    case OpCode::SUBTRACT:
        return simple_instruction("SUBTRACT", offset);
    case OpCode::MULTIPLY:
        return simple_instruction("MULTIPLY", offset);
    case OpCode::DIVIDE:
        return simple_instruction("DIVIDE", offset);
    case OpCode::TRUE:
        return simple_instruction("TRUE", offset);
    case OpCode::FALSE:
        return simple_instruction("FALSE", offset);
    case OpCode::NIL:
        return simple_instruction("NIL", offset);
    case OpCode::NOT:
        return simple_instruction("NOT", offset);
    case OpCode::EQUAL:
        return simple_instruction("EQUAL", offset);
    case OpCode::GREATER:
        return simple_instruction("GREATER", offset);
    case OpCode::LESS:
        return simple_instruction("LESS", offset);
    default:
        std::println("Unknown instruction {}", static_cast<uint8_t>(instruction));
        return offset + 1;
    }
}

int Chunk::add_constant(Value value)
{
    _constants.push_back(value);
    return _constants.size() - 1;
}

const uint8_t* Chunk::get_code() const
{
    return _code.data();
}

void Chunk::disassemble_instruction(const uint8_t* instruction) const
{
    _disassemble_instruction(instruction - _code.data());
}

} // namespace lox