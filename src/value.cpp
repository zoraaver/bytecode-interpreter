#include "value.h"
#include "object.h"

#include <print>

namespace lox
{

std::string Value::to_string() const
{
    switch(type)
    {
    case ValueType::BOOL:
        return as.boolean ? "true" : "false";
    case ValueType::NUMBER:
        return std::to_string(as.number);
    case ValueType::NIL:
        return "nil";
    case ValueType::OBJECT:
        return as.obj->to_string();
    }
}

} // namespace lox