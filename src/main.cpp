#include "chunk.h"
#include "vm.h"

int main(int argc, const char* argv[])
{
    lox::Chunk chunk;

    chunk.write(lox::OpCode::CONSTANT, 123);
    chunk.write(chunk.add_constant(1.2), 123);

    chunk.write(lox::OpCode::CONSTANT, 123);
    chunk.write(chunk.add_constant(3.4), 123);

    chunk.write(lox::OpCode::ADD, 123);

    chunk.write(lox::OpCode::CONSTANT, 123);
    chunk.write(chunk.add_constant(5.6), 123);

    chunk.write(lox::OpCode::DIVIDE, 123);

    chunk.write(lox::OpCode::NEGATE, 123);

    chunk.write(lox::OpCode::RETURN, 123);

    lox::VM vm{chunk};
    vm.interpret();
    return 0;
}