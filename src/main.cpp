#include "chunk.h"

int main(int argc, const char* argv[])
{
    lox::Chunk chunk;
    chunk.write(lox::OpCode::RETURN);
    chunk.disassemble("test chunk");
    return 0;
}