#include "chunk.h"

int main(int argc, const char* argv[])
{
    lox::Chunk chunk;
    auto index = chunk.add_constant(1.3);
    chunk.write(lox::OpCode::CONSTANT, 123);
    chunk.write(index, 123);
    chunk.write(lox::OpCode::RETURN, 123);
    chunk.disassemble("test chunk");
    return 0;
}