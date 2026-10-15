#include "rules.hpp"

#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

#include "json.hpp"

namespace {

using json::Value;

[[noreturn]] void bad(std::size_t index, const std::string& msg) {
    throw std::runtime_error("rule #" + std::to_string(index + 1) + ": " + msg);
}

const Value& need(const Value& obj, const std::string& key, Value::Type type, const char* typeName, std::size_t index) {
    const auto it = obj.object.find(key);
    if (it == obj.object.end() || it->second.type != type)
        bad(index, "field '" + key + "' is required and must be " + typeName);
    return it->second;
}

std::string needString(const Value& obj, const std::string& key, std::size_t i) {
    return need(obj, key, Value::Type::String, "a string", i).str;
}

int needWindow(const Value& obj, const std::string& key, std::size_t i) {
    const double d = need(obj, key, Value::Type::Number, "a number", i).number;
    if (d != std::floor(d) || d < 1 || d > 1e6) bad(i, "'" + key + "' must be a whole number between 1 and 1000000");
    return static_cast<int>(d);
}

}  // namespace

std::vector<Rule> parseRules(const std::string& jsonText) {
    const Value root = json::parse(jsonText);
    if (root.type != Value::Type::Array) throw std::runtime_error("rules file must contain a JSON array of rules");

    std::vector<Rule> rules;
    std::set<std::string> seenIds;
    for (std::size_t i = 0; i < root.array.size(); ++i) {
        const Value& obj = root.array[i];
        if (obj.type != Value::Type::Object) bad(i, "must be a JSON object");

        Rule r;
        r.id = needString(obj, "id", i);
        r.instrument = needString(obj, "instrument", i);
        if (r.id.empty() || r.instrument.empty()) bad(i, "'id' and 'instrument' must not be empty");
        if (!seenIds.insert(r.id).second) bad(i, "duplicate id '" + r.id + "'");

        const std::string dir = needString(obj, "direction", i);
        if (dir == "above") r.direction = Direction::Above;
        else if (dir == "below") r.direction = Direction::Below;
        else bad(i, "'direction' must be \"above\" or \"below\", got \"" + dir + "\"");

        const std::string type = needString(obj, "type", i);
        if (type == "level") {
            r.type = RuleType::Level;
            r.level = need(obj, "level", Value::Type::Number, "a number", i).number;
        } else if (type == "ma_cross") {
            r.type = RuleType::MaCross;
            r.shortWindow = needWindow(obj, "short", i);
            r.longWindow = needWindow(obj, "long", i);
            if (r.shortWindow >= r.longWindow) bad(i, "'short' must be smaller than 'long'");
        } else {
            bad(i, "unknown type \"" + type + "\" (expected \"level\" or \"ma_cross\")");
        }
        rules.push_back(r);
    }
    return rules;
}

std::vector<Rule> loadRules(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open rules file '" + path + "'");
    std::stringstream buffer;
    buffer << in.rdbuf();
    return parseRules(buffer.str());
}
