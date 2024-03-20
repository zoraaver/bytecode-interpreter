#ifndef LOX_VALUE_H
#define LOX_VALUE_H

namespace lox
{
enum class ValueType
{
    BOOL,
    NIL,
    NUMBER
};

class Value
{
    ValueType type;
    union
    {
        bool boolean;
        double number;
    } as;

public:
    Value(bool boolean)
        : type(ValueType::BOOL)
        , as{.boolean = boolean}
    { }

    Value(double number)
        : type(ValueType::NUMBER)
        , as{.number = number}
    { }

    Value()
        : type(ValueType::NIL)
    { }

    ValueType get_type() const
    {
        return type;
    }

    bool is_bool() const
    {
        return type == ValueType::BOOL;
    }

    bool is_number() const
    {
        return type == ValueType::NUMBER;
    }

    bool is_nil() const
    {
        return type == ValueType::NIL;
    }

    bool as_bool() const
    {
        return as.boolean;
    }

    double as_number() const
    {
        return as.number;
    }

    void negate()
    {
        as.number = -as.number;
    }

    void not_op()
    {
        as.boolean = !is_falsey();
        type = ValueType::BOOL;
    }

    Value& operator=(bool boolean)
    {
        type = ValueType::BOOL;
        as.boolean = boolean;
        return *this;
    }

    Value& operator=(double number)
    {
        type = ValueType::NUMBER;
        as.number = number;
        return *this;
    }

    bool operator==(const Value& other) const
    {
        if(type != other.type)
            return false;

        switch(type)
        {
        case ValueType::BOOL:
            return as.boolean == other.as.boolean;
        case ValueType::NUMBER:
            return as.number == other.as.number;
        case ValueType::NIL:
            return true;
        }
    }

    bool is_falsey()
    {
        return is_nil() || (is_bool() && !as_bool());
    }
};

void print_value(Value);
} // namespace lox

#endif // LOX_VALUE_H