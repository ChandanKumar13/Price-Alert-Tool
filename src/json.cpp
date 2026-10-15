#include "json.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string_view>

namespace json {
namespace {

constexpr int kMaxDepth = 64;  // stops absurdly nested input from overflowing the stack

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text) {}

    Value parseDocument() {
        Value v = parseValue();
        skipSpace();
        if (pos_ != s_.size()) fail("unexpected text after the JSON value");
        return v;
    }

private:
    const std::string& s_;
    std::size_t pos_ = 0;
    int depth_ = 0;

    [[noreturn]] void fail(const std::string& msg) const {
        throw std::runtime_error("invalid JSON at character " + std::to_string(pos_) + ": " + msg);
    }
    void skipSpace() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }
    char peek() {
        skipSpace();
        if (pos_ >= s_.size()) fail("unexpected end of input");
        return s_[pos_];
    }
    void expect(char c) {
        if (peek() != c) fail(std::string("expected '") + c + "'");
        ++pos_;
    }
    bool consumeWord(const char* word) {
        const std::size_t n = std::char_traits<char>::length(word);
        if (s_.compare(pos_, n, word) != 0) return false;
        pos_ += n;
        return true;
    }
    // After one array/object element: eat ',' (more to come) or the closing bracket.
    bool more(char close) {
        const char c = peek();
        if (c == ',') { ++pos_; return true; }
        if (c == close) { ++pos_; return false; }
        fail(std::string("expected ',' or '") + close + "'");
    }
    void enter() { if (++depth_ > kMaxDepth) fail("nesting too deep"); }

    Value parseValue() {
        const char c = peek();
        Value v;
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') { v.type = Value::Type::String; v.str = parseString(); return v; }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber();
        if (consumeWord("true"))  { v.type = Value::Type::Bool; v.boolean = true; return v; }
        if (consumeWord("false")) { v.type = Value::Type::Bool; return v; }
        if (consumeWord("null"))  { return v; }
        fail("unexpected character");
    }

    Value parseArray() {
        enter();
        Value v;
        v.type = Value::Type::Array;
        expect('[');
        if (peek() == ']') ++pos_;
        else do { v.array.push_back(parseValue()); } while (more(']'));
        --depth_;
        return v;
    }

    Value parseObject() {
        enter();
        Value v;
        v.type = Value::Type::Object;
        expect('{');
        if (peek() == '}') ++pos_;
        else do {
            if (peek() != '"') fail("expected a string key");
            std::string key = parseString();
            expect(':');
            v.object[key] = parseValue();
        } while (more('}'));
        --depth_;
        return v;
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (pos_ < s_.size()) {
            const char c = s_[pos_++];
            if (c == '"') return out;
            if (c != '\\') { out += c; continue; }
            if (pos_ >= s_.size()) break;
            switch (s_[pos_++]) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                default: --pos_; fail("unsupported escape sequence (\\u is not supported)");
            }
        }
        fail("unterminated string");
    }

    Value parseNumber() {
        const std::size_t start = pos_;
        while (pos_ < s_.size() && std::string_view("+-0123456789.eE").find(s_[pos_]) != std::string_view::npos) ++pos_;
        const std::string token = s_.substr(start, pos_ - start);
        char* end = nullptr;
        const double d = std::strtod(token.c_str(), &end);
        if (end != token.c_str() + token.size() || !std::isfinite(d)) { pos_ = start; fail("malformed number"); }
        Value v;
        v.type = Value::Type::Number;
        v.number = d;
        return v;
    }
};

}  // namespace

Value parse(const std::string& text) { return Parser(text).parseDocument(); }

}  // namespace json
