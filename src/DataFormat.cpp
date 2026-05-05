#include "DataFormat.hpp"

#include <cassert>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace DataFormat {
    const Value* Object::Get(const std::string& key) const {
        for (const auto& [k, v] : entries) {
            if (k == key) return v.get();
        }
        return nullptr;
    }

    const Value* Document::Get(const std::string& key) const {
        for (const auto& [k, v] : entries) {
            if (k == key) return &v;
        }
        return nullptr;
    }

    namespace {
        enum class TK {
            Ident,
            Integer,
            Float,
            String,
            DotDot,
            Colon,
            Comma,
            LBracket,
            RBracket,
            LBrace,
            RBrace,
            Newline,
            End,
            Error,
            Keybind
        };

        struct Token {
            TK          kind;
            std::string text;
            int         line = 1;
        };

        struct Lexer {
            const std::string& src;
            std::size_t        pos  = 0;
            int                line = 1;

            explicit Lexer(const std::string& s) : src(s) {}

            Token peek() {
                const std::size_t savedPos  = pos;
                const int         savedLine = line;
                Token t = next();
                pos  = savedPos;
                line = savedLine;
                return t;
            }

            Token next() {
                while (pos < src.size() &&
                    (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r'))
                    ++pos;

                if (pos >= src.size()) return {TK::End, "", line};

                const char c = src[pos];

                if (c == '\n') { ++pos; ++line; return {TK::Newline, "\n", line}; }

                if (c == '#') {
                    while (pos < src.size() && src[pos] != '\n') ++pos;
                    return next();
                }

                if (c == ':') { ++pos; return {TK::Colon,    ":",  line}; }
                if (c == ',') { ++pos; return {TK::Comma,    ",",  line}; }
                if (c == '[') { ++pos; return {TK::LBracket, "[",  line}; }
                if (c == ']') { ++pos; return {TK::RBracket, "]",  line}; }
                if (c == '{') { ++pos; return {TK::LBrace,   "{",  line}; }
                if (c == '}') { ++pos; return {TK::RBrace,   "}",  line}; }

                if (c == '<') return scanKeybind();

                if (c == '.') {
                    if (pos + 1 < src.size() && src[pos + 1] == '.') {
                        pos += 2;
                        return {TK::DotDot, "..", line};
                    }
                    ++pos;
                    return {TK::Error, ".", line};
                }

                if (c == '"')  return scanString();

                if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
                    return scanIdent();

                const bool startsNumber =
                    std::isdigit(static_cast<unsigned char>(c)) ||
                    (c == '-' && pos + 1 < src.size() &&
                    std::isdigit(static_cast<unsigned char>(src[pos + 1])));
                if (startsNumber) return scanNumber();

                ++pos;
                return {TK::Error, std::string(1, c), line};
            }

        private:
            Token scanString() {
                const int startLine = line;
                ++pos;
                std::string result;
                while (pos < src.size() && src[pos] != '"') {
                    if (src[pos] == '\n') break;
                    if (src[pos] == '\\' && pos + 1 < src.size()) {
                        ++pos;
                        switch (src[pos]) {
                            case 'n':  result += '\n'; break;
                            case 't':  result += '\t'; break;
                            case '"':  result += '"';  break;
                            case '\\': result += '\\'; break;
                            default:   result += '\\'; result += src[pos]; break;
                        }
                    } else {
                        result += src[pos];
                    }
                    ++pos;
                }
                if (pos < src.size() && src[pos] == '"') ++pos;
                return {TK::String, std::move(result), startLine};
            }

            Token scanKeybind() {
                const int startLine = line;
                ++pos; // consume '<'
                std::string raw;
                while (pos < src.size() && src[pos] != '>' && src[pos] != '\n') {
                    if (src[pos] == '\\' && pos + 1 < src.size()) {
                        raw += '\\';
                        raw += src[pos + 1];
                        pos += 2;
                    } else {
                        raw += src[pos];
                        ++pos;
                    }
                }
                if (pos < src.size() && src[pos] == '>') ++pos; // consume '>'
                return {TK::Keybind, std::move(raw), startLine};
            }

            Token scanIdent() {
                const std::size_t start = pos;
                while (pos < src.size() &&
                    (std::isalnum(static_cast<unsigned char>(src[pos])) ||
                        src[pos] == '_'))
                    ++pos;
                return {TK::Ident, src.substr(start, pos - start), line};
            }

            Token scanNumber() {
                const std::size_t start   = pos;
                bool              isFloat = false;

                if (src[pos] == '-') ++pos;

                while (pos < src.size() &&
                    std::isdigit(static_cast<unsigned char>(src[pos])))
                    ++pos;

                if (pos < src.size() && src[pos] == '.') {
                    const bool nextIsDot   = (pos + 1 < src.size() && src[pos + 1] == '.');
                    const bool nextIsDigit =
                        (pos + 1 < src.size() &&
                        std::isdigit(static_cast<unsigned char>(src[pos + 1])));
                    if (!nextIsDot && nextIsDigit) {
                        isFloat = true;
                        ++pos;  // consume '.'
                        while (pos < src.size() &&
                            std::isdigit(static_cast<unsigned char>(src[pos])))
                            ++pos;
                    }
                }

                if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
                    isFloat = true;
                    ++pos;
                    if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) ++pos;
                    while (pos < src.size() &&
                        std::isdigit(static_cast<unsigned char>(src[pos])))
                        ++pos;
                }

                return {isFloat ? TK::Float : TK::Integer,
                        src.substr(start, pos - start),
                        line};
            }
        };

        struct Parser {
            Lexer       lex;
            std::string error;
            Token       current;

            explicit Parser(const std::string& src) : lex(src), current(lex.next()) {}

            bool failed() const { return !error.empty(); }

            void advance() { current = lex.next(); }

            void skipNewlines() {
                while (current.kind == TK::Newline) advance();
            }

            bool expect(TK kind, const char* what) {
                if (current.kind != kind) {
                    error = "line " + std::to_string(current.line) +
                            ": expected " + what +
                            ", got '" + current.text + "'";
                    return false;
                }
                advance();
                return true;
            }

            bool parseNumericToken(double& outVal, bool& outIsFloat) {
                if (current.kind == TK::Integer) {
                    outIsFloat = false;
                    try { outVal = static_cast<double>(std::stoll(current.text)); }
                    catch (...) {
                        error = "line " + std::to_string(current.line) +
                                ": integer out of range '" + current.text + "'";
                        return false;
                    }
                    advance();
                    return true;
                }
                if (current.kind == TK::Float) {
                    outIsFloat = true;
                    try { outVal = std::stod(current.text); }
                    catch (...) {
                        error = "line " + std::to_string(current.line) +
                                ": float out of range '" + current.text + "'";
                        return false;
                    }
                    advance();
                    return true;
                }
                error = "line " + std::to_string(current.line) +
                        ": expected a number, got '" + current.text + "'";
                return false;
            }

            Value parseValue() {
                if (failed()) return Value{};

                if (current.kind == TK::Keybind) {
                    return parseKeybind();
                }

                if (current.kind == TK::Integer || current.kind == TK::Float) {
                    double loVal = 0.0;
                    bool   loFlt = false;
                    if (!parseNumericToken(loVal, loFlt)) return Value{};

                    if (current.kind == TK::DotDot) {
                        advance();  // consume '..'
                        double hiVal = 0.0;
                        bool   hiFlt = false;
                        if (!parseNumericToken(hiVal, hiFlt)) return Value{};

                        if (loFlt || hiFlt)
                            return Value(FloatRange{loVal, hiVal});
                        else
                            return Value(IntRange{static_cast<int64_t>(loVal),
                                                static_cast<int64_t>(hiVal)});
                    }

                    if (loFlt) return Value(loVal);
                    return Value(static_cast<int64_t>(loVal));
                }

                if (current.kind == TK::String) {
                    std::string s = current.text;
                    advance();
                    return Value(std::move(s));
                }

                if (current.kind == TK::Ident) {
                    std::string name = current.text;
                    advance();

                    if (name == "true")  return Value(true);
                    if (name == "false") return Value(false);

                    if (current.kind == TK::LBracket) {
                        TypedArray::ElemType elemType;
                        if      (name == "int")    elemType = TypedArray::ElemType::Int;
                        else if (name == "float")  elemType = TypedArray::ElemType::Float;
                        else if (name == "string" || name == "str") elemType = TypedArray::ElemType::String;
                        else if (name == "bool")   elemType = TypedArray::ElemType::Bool;
                        else if (name == "tag")    elemType = TypedArray::ElemType::Tag;
                        else {
                            error = "line " + std::to_string(current.line) +
                                    ": unknown typed-array element type '" + name + "'";
                            return Value{};
                        }

                        advance();
                        TypedArray arr;
                        arr.elemType = elemType;

                        while (current.kind != TK::RBracket &&
                            current.kind != TK::End &&
                            !failed()) {
                            Value elem = parseValue();
                            if (failed()) return Value{};
                            arr.elements.push_back(
                                std::make_shared<Value>(std::move(elem)));

                            if (current.kind == TK::Comma) {
                                advance();
                                if (current.kind == TK::RBracket) break;
                            } else {
                                break;
                            }
                        }
                        if (!expect(TK::RBracket, "]")) return Value{};
                        return Value(std::move(arr));
                    }

                    return Value(Tag{std::move(name)});
                }

                if (current.kind == TK::LBrace) {
                    advance();
                    Object obj;
                    skipNewlines();

                    while (current.kind != TK::RBrace &&
                        current.kind != TK::End &&
                        !failed()) {
                        if (current.kind != TK::Ident) {
                            error = "line " + std::to_string(current.line) +
                                    ": expected identifier as object key, got '" +
                                    current.text + "'";
                            return Value{};
                        }
                        std::string key = current.text;
                        advance();
                        if (!expect(TK::Colon, ":")) return Value{};

                        Value val = parseValue();
                        if (failed()) return Value{};

                        obj.entries.push_back(
                            {std::move(key), std::make_shared<Value>(std::move(val))});

                        skipNewlines();
                        if (current.kind == TK::Comma) {
                            advance();
                            skipNewlines();
                            if (current.kind == TK::RBrace) break;
                        }
                    }
                    if (!expect(TK::RBrace, "}")) return Value{};
                    return Value(std::move(obj));
                }

                error = "line " + std::to_string(current.line) +
                        ": unexpected token '" + current.text + "' in value position";
                return Value{};
            }

            Value parseKeybind() {
                const std::string raw = current.text;
                advance();
                Keybind kb;
                std::string tok;
                for (std::size_t i = 0; i < raw.size(); ) {
                    if (raw[i] == '\\' && i + 1 < raw.size()) {
                        tok += raw[i + 1];
                        i += 2;
                    } else if (raw[i] == '+') {
                        if (!tok.empty()) { kb.keys.push_back(tok); tok.clear(); }
                        ++i;
                    } else {
                        tok += raw[i];
                        ++i;
                    }
                }
                if (!tok.empty()) kb.keys.push_back(tok);
                return Value(std::move(kb));
            }

            std::optional<Document> parseDocument() {
                Document doc;
                skipNewlines();

                while (current.kind != TK::End && !failed()) {
                    if (current.kind != TK::Ident) {
                        error = "line " + std::to_string(current.line) +
                                ": expected 'identifier :' entry, got '" +
                                current.text + "'";
                        return std::nullopt;
                    }

                    std::string key = current.text;
                    advance();
                    if (!expect(TK::Colon, ":")) return std::nullopt;

                    Value val = parseValue();
                    if (failed()) return std::nullopt;

                    doc.entries.push_back({std::move(key), std::move(val)});

                    while (current.kind == TK::Newline) advance();
                }

                if (failed()) return std::nullopt;
                return doc;
            }
        };
    }

    std::optional<Document> ParseString(const std::string& source, std::string* errorMsg) {
        Parser p(source);
        auto doc = p.parseDocument();
        if (!doc && errorMsg) *errorMsg = p.error;
        return doc;
    }

    std::optional<Document> ParseFile(const std::string& path, std::string* errorMsg) {
        std::ifstream f(path);
        if (!f.is_open()) {
            if (errorMsg) *errorMsg = "cannot open file: " + path;
            return std::nullopt;
        }
        std::ostringstream ss;
        ss << f.rdbuf();
        return ParseString(ss.str(), errorMsg);
    }
}