#ifndef LOX_COMPILER_H
#define LOX_COMPILER_H

#include <string_view>

#include "scanner.h"

namespace lox
{
class Compiler
{
    Scanner& _scanner;

public:
    Compiler(Scanner& scanner);
    void compile();
};
} // namespace lox

#endif // LOX_COMPILER_H