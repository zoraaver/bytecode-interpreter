#ifndef LOX_CHUNK_H
#define LOX_CHUNK_H

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "value.h"

namespace lox
{

enum class OpCode : uint8_t
{
    RETURN,
    POP,
    DEFINE_GLOBAL,
    GET_GLOBAL,
    SET_GLOBAL,
    GET_LOCAL,
    SET_LOCAL,
    CONSTANT,
    NIL,
    TRUE,
    FALSE,
    NOT,
    NEGATE,
    EQUAL,
    GREATER,
    LESS,
    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE,
    JUMP_IF_FALSE,
    JUMP_IF_TRUE,
    JUMP,
    LOOP,
    CALL,
    CLOSURE,
    GET_UPVALUE,
    SET_UPVALUE,
    CLOSE_UPVALUE,
    CLASS,
    GET_PROPERTY,
    SET_PROPERTY,
    METHOD,
    INVOKE,
    INHERIT,
    GET_SUPER
};

class Chunk
{
    std::vector<uint8_t> _code;
    std::vector<Value> _constants;
    std::vector<int> _lines;

    int _disassemble_instruction(int offset);
    int _constant_instruction(std::string_view name, int offset) const;
    int _byte_instruction(std::string_view name, int offset) const;
    int _jump_instruction(std::string_view name, int sign, int offset) const;
    int _invoke_instruction(std::string_view name, int offset) const;

public:
    void disassemble(std::string_view name);
    void write(OpCode, int line);
    void write(uint8_t byte, int line);
    int add_constant(const Value&);
    const uint8_t* get_code() const;

    std::vector<Value>& get_constants()
    {
        return _constants;
    }

    const std::vector<Value>& get_constants() const
    {
        return _constants;
    }

    const Value& get_constant(size_t index) const
    {
        return _constants[index];
    };

    Value& get_constant(size_t index)
    {
        return _constants[index];
    };

    int get_line(size_t index) const
    {
        return _lines[index];
    }

    size_t size() const
    {
        return _code.size();
    }

    uint8_t& operator[](size_t index)
    {
        return _code[index];
    }

    const uint8_t& operator[](size_t index) const
    {
        return _code[index];
    }

    void disassemble_instruction(const uint8_t*);
};

} // namespace lox

#endif // LOX_CHUNK_H