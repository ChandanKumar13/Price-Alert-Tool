// Minimal JSON reader: just enough for the rules file (objects, arrays,
// strings, numbers, true/false/null). Throws std::runtime_error on bad input.
#pragma once
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace json {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string str;
    std::vector<Value> array;
    std::map<std::string, Value> object;
};

Value parse(const std::string& text);

}  // namespace json
