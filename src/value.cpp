#include "value.h"

#include <print>

namespace lox
{

void print_value(Value value)
{
    switch(value.get_type())
    {
    case ValueType::BOOL:
        std::print("'{}'", value.as_bool() ? "true" : "false");
        return;
    case ValueType::NUMBER:
        std::print("'{:g}'", value.as_number());
        return;
    case ValueType::NIL:
        std::print("nil");
        return;
    }
}

} // namespace lox