#include "value.h"

#include <print>

namespace lox
{

void print_value(Value value)
{
    std::print("'{:g}'", value);
}

} // namespace lox