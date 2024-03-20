#include "value.h"
#include "object.h"

#include <print>

namespace lox
{

void print_value(Value value)
{
    switch(value.get_type())
    {
    case ValueType::BOOL:
        std::println("'{}'", value.as_bool() ? "true" : "false");
        return;
    case ValueType::NUMBER:
        std::println("'{:g}'", value.as_number());
        return;
    case ValueType::NIL:
        std::println("nil");
        return;
    case ValueType::OBJECT:
        if(const auto* str = value.as_object<StringObject>())
        {
            std::println("'{}'", str->value());
        }
        else
        {
            throw std::runtime_error("print_value: object is not a string");
        }
        return;
    }
}

} // namespace lox