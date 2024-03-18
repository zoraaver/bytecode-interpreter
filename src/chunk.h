#ifndef LOX_CHUNK_H
#define LOX_CHUNK_H

#include <cstdint>
#include <string_view>
#include <vector>
namespace lox
{

enum class OpCode : uint8_t
{
    RETURN
};

class Chunk
{
    std::vector<uint8_t> _code;

    int _disassemble_instruction(int offset) const;

public:
    void disassemble(std::string_view name) const;
    void write(OpCode);
};

} // namespace lox

#endif // LOX_CHUNK_H