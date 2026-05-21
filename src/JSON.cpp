#include "JSON.hpp"

#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

namespace JSON {
    const Value* Object::Find(const std::string& key) const {
        for(const auto& [k, v] : entries) {
            if(k == key) {
                return &v;
            }
        }
        return nullptr;
    }

    namespace {
        struct Parser {
            const std::string& src;
            std::size_t pos = 0;
            std::string error;

            explicit Parser(const std::string& text) : src(text) {}

            void skipWhitespace() {
                while(pos < src.size()) {
                    const char c = src[pos];
                    if(c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                        ++pos;
                        continue;
                    }
                    break;
                }
            }

            bool consume(char expected) {
                skipWhitespace();
                if(pos >= src.size() || src[pos] != expected) {
                    setError(std::string("expected '") + expected + "'");
                    return false;
                }
                ++pos;
                return true;
            }

            void setError(const std::string& message) {
                if(error.empty()) {
                    error = message + " at byte " + std::to_string(pos);
                }
            }

            std::optional<Value> parseValue() {
                skipWhitespace();
                if(pos >= src.size()) {
                    setError("unexpected end of input");
                    return std::nullopt;
                }

                const char c = src[pos];
                if(c == '{') return parseObject();
                if(c == '[') return parseArray();
                if(c == '"') return parseString();
                if(c == 't' || c == 'f') return parseBool();
                if(c == 'n') return parseNull();
                if(c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber();

                setError(std::string("unexpected character '") + c + "'");
                return std::nullopt;
            }

            std::optional<Value> parseNull() {
                if(src.compare(pos, 4, "null") != 0) {
                    setError("invalid null literal");
                    return std::nullopt;
                }
                pos += 4;
                return Value{};
            }

            std::optional<Value> parseBool() {
                if(src.compare(pos, 4, "true") == 0) {
                    pos += 4;
                    return Value(true);
                }
                if(src.compare(pos, 5, "false") == 0) {
                    pos += 5;
                    return Value(false);
                }
                setError("invalid boolean literal");
                return std::nullopt;
            }

            std::optional<Value> parseNumber() {
                const std::size_t start = pos;
                if(src[pos] == '-') {
                    ++pos;
                }
                while(pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) {
                    ++pos;
                }
                if(pos < src.size() && src[pos] == '.') {
                    ++pos;
                    while(pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) {
                        ++pos;
                    }
                }
                if(pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
                    ++pos;
                    if(pos < src.size() && (src[pos] == '+' || src[pos] == '-')) {
                        ++pos;
                    }
                    while(pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) {
                        ++pos;
                    }
                }
                try {
                    return Value(std::stod(src.substr(start, pos - start)));
                } catch(...) {
                    setError("invalid number literal");
                    return std::nullopt;
                }
            }

            std::optional<Value> parseString() {
                if(src[pos] != '"') {
                    setError("expected string");
                    return std::nullopt;
                }
                ++pos;
                std::string out;
                while(pos < src.size()) {
                    const char c = src[pos++];
                    if(c == '"') {
                        return Value(std::move(out));
                    }
                    if(c == '\\') {
                        if(pos >= src.size()) {
                            setError("unterminated escape sequence");
                            return std::nullopt;
                        }
                        const char esc = src[pos++];
                        switch(esc) {
                            case '"': out += '"'; break;
                            case '\\': out += '\\'; break;
                            case '/': out += '/'; break;
                            case 'b': out += '\b'; break;
                            case 'f': out += '\f'; break;
                            case 'n': out += '\n'; break;
                            case 'r': out += '\r'; break;
                            case 't': out += '\t'; break;
                            default:
                                setError("unsupported escape sequence");
                                return std::nullopt;
                        }
                    } else {
                        out += c;
                    }
                }
                setError("unterminated string");
                return std::nullopt;
            }

            std::optional<Value> parseArray() {
                if(!consume('[')) return std::nullopt;
                auto arr = std::make_shared<Array>();
                skipWhitespace();
                if(pos < src.size() && src[pos] == ']') {
                    ++pos;
                    return Value(arr);
                }
                while(true) {
                    auto value = parseValue();
                    if(!value) return std::nullopt;
                    arr->values.push_back(std::move(*value));
                    skipWhitespace();
                    if(pos < src.size() && src[pos] == ',') {
                        ++pos;
                        continue;
                    }
                    if(pos < src.size() && src[pos] == ']') {
                        ++pos;
                        break;
                    }
                    setError("expected ',' or ']'");
                    return std::nullopt;
                }
                return Value(arr);
            }

            std::optional<Value> parseObject() {
                if(!consume('{')) return std::nullopt;
                auto obj = std::make_shared<Object>();
                skipWhitespace();
                if(pos < src.size() && src[pos] == '}') {
                    ++pos;
                    return Value(obj);
                }
                while(true) {
                    skipWhitespace();
                    auto keyValue = parseString();
                    if(!keyValue || !keyValue->IsString()) {
                        if(error.empty()) {
                            setError("expected object key string");
                        }
                        return std::nullopt;
                    }
                    const std::string key = keyValue->AsString();
                    if(!consume(':')) return std::nullopt;
                    auto value = parseValue();
                    if(!value) return std::nullopt;
                    obj->entries.emplace_back(key, std::move(*value));
                    skipWhitespace();
                    if(pos < src.size() && src[pos] == ',') {
                        ++pos;
                        continue;
                    }
                    if(pos < src.size() && src[pos] == '}') {
                        ++pos;
                        break;
                    }
                    setError("expected ',' or '}'");
                    return std::nullopt;
                }
                return Value(obj);
            }
        };
    }

    std::optional<Value> ParseString(const std::string& source, std::string* errorMsg) {
        Parser parser(source);
        auto value = parser.parseValue();
        if(value) {
            parser.skipWhitespace();
            if(parser.pos != source.size()) {
                parser.setError("trailing characters after JSON value");
                value.reset();
            }
        }
        if(!value && errorMsg) {
            *errorMsg = parser.error.empty() ? "unknown JSON parse error" : parser.error;
        }
        if(value && errorMsg) {
            errorMsg->clear();
        }
        return value;
    }

    std::optional<Value> ParseFile(const std::string& path, std::string* errorMsg) {
        std::ifstream file(path, std::ios::binary);
        if(!file) {
            if(errorMsg) {
                *errorMsg = "failed to open JSON file: " + path;
            }
            return std::nullopt;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        return ParseString(ss.str(), errorMsg);
    }
}
