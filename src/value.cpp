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
        std::print("'{}'", value.as_bool() ? "true" : "false");
        return;
    case ValueType::NUMBER:
        std::print("'{:g}'", value.as_number());
        return;
    case ValueType::NIL:
        std::print("nil");
        return;
    case ValueType::OBJECT:
        if(const auto* str = value.as_object<StringObject>())
        {
            std::print("'{}'", str->value());
        }
        else
        {
            throw std::runtime_error("print_value: object is not a string");
        }
        return;
    }
}

} // namespace lox