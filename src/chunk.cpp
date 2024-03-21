#include "chunk.h"

#include <cstdint>
#include <print>
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

    return offset + 2;
}

int Chunk::_byte_instruction(std::string_view name, int offset) const
{
    auto slot = _code.at(offset + 1);
    std::println("{:16} {:4d}", name, slot);

    return offset + 2;
}

int Chunk::_jump_instruction(std::string_view name, int sign, int offset) const
{
    uint16_t jump = static_cast<uint16_t>(_code.at(offset + 1)) << 8;
    jump |= _code.at(offset + 2);

    std::println("{:16} {:4d} -> {:d}", name, offset, offset + 3 + sign * jump);

    return offset + 3;
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
#define INSTRUCTION(name, type)                                                                    \
    case OpCode::name:                                                                             \
        return type(#name, offset);
#define INSTRUCTION_1(name, type, sign)                                                            \
    case OpCode::name:                                                                             \
        return type(#name, sign, offset);
        INSTRUCTION(RETURN, simple_instruction)
        INSTRUCTION(CONSTANT, _constant_instruction)
        INSTRUCTION(NEGATE, simple_instruction)
        INSTRUCTION(ADD, simple_instruction)
        INSTRUCTION(SUBTRACT, simple_instruction)
        INSTRUCTION(MULTIPLY, simple_instruction)
        INSTRUCTION(DIVIDE, simple_instruction)
        INSTRUCTION(TRUE, simple_instruction)
        INSTRUCTION(FALSE, simple_instruction)
        INSTRUCTION(NIL, simple_instruction)
        INSTRUCTION(NOT, simple_instruction)
        INSTRUCTION(EQUAL, simple_instruction)
        INSTRUCTION(GREATER, simple_instruction)
        INSTRUCTION(LESS, simple_instruction)
        INSTRUCTION(PRINT, simple_instruction)
        INSTRUCTION(POP, simple_instruction)
        INSTRUCTION(DEFINE_GLOBAL, simple_instruction)
        INSTRUCTION(GET_GLOBAL, simple_instruction)
        INSTRUCTION(SET_GLOBAL, simple_instruction)
        INSTRUCTION(GET_LOCAL, _byte_instruction)
        INSTRUCTION(SET_LOCAL, _byte_instruction)
        INSTRUCTION_1(JUMP_IF_FALSE, _jump_instruction, 1)
        INSTRUCTION_1(JUMP, _jump_instruction, 1)
#undef INSTRUCTION_1
#undef INSTRUCTION
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