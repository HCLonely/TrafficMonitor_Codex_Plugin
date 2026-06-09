#include "JsonLite.h"

#include <charconv>
#include <cstdlib>

namespace jsonlite {

Value::Value(std::nullptr_t) : storage_(nullptr) {}
Value::Value(bool value) : storage_(value) {}
Value::Value(double value) : storage_(value) {}
Value::Value(std::string value) : storage_(std::move(value)) {}
Value::Value(Object value) : storage_(std::move(value)) {}
Value::Value(Array value) : storage_(std::move(value)) {}

bool Value::IsNull() const { return std::holds_alternative<std::nullptr_t>(storage_); }
bool Value::IsBool() const { return std::holds_alternative<bool>(storage_); }
bool Value::IsNumber() const { return std::holds_alternative<double>(storage_); }
bool Value::IsString() const { return std::holds_alternative<std::string>(storage_); }
bool Value::IsObject() const { return std::holds_alternative<Object>(storage_); }
bool Value::IsArray() const { return std::holds_alternative<Array>(storage_); }

std::optional<bool> Value::AsBool() const {
    if (!IsBool()) {
        return std::nullopt;
    }
    return std::get<bool>(storage_);
}

std::optional<double> Value::AsNumber() const {
    if (!IsNumber()) {
        return std::nullopt;
    }
    return std::get<double>(storage_);
}

std::optional<int> Value::AsInt() const {
    auto number = AsNumber();
    if (!number.has_value()) {
        return std::nullopt;
    }
    return static_cast<int>(*number);
}

std::optional<std::string_view> Value::AsString() const {
    if (!IsString()) {
        return std::nullopt;
    }
    return std::get<std::string>(storage_);
}

const Value::Object* Value::AsObject() const {
    return IsObject() ? &std::get<Object>(storage_) : nullptr;
}

const Value::Array* Value::AsArray() const {
    return IsArray() ? &std::get<Array>(storage_) : nullptr;
}

const Value* Value::Find(std::string_view key) const {
    const Object* object = AsObject();
    if (object == nullptr) {
        return nullptr;
    }

    auto it = object->find(std::string(key));
    if (it == object->end()) {
        return nullptr;
    }
    return &it->second;
}

Parser::Parser(std::string_view text) : text_(text) {}

std::optional<Value> Parser::Parse() {
    SkipWhitespace();
    std::optional<Value> value = ParseValue();
    if (!value.has_value()) {
        return std::nullopt;
    }

    SkipWhitespace();
    if (!IsAtEnd()) {
        SetError("unexpected trailing characters");
        return std::nullopt;
    }

    return value;
}

const std::string& Parser::Error() const {
    return error_;
}

std::optional<Value> Parser::ParseValue() {
    SkipWhitespace();
    if (IsAtEnd()) {
        SetError("unexpected end of input");
        return std::nullopt;
    }

    const char ch = Peek();
    switch (ch) {
        case '{':
            return ParseObject();
        case '[':
            return ParseArray();
        case '"': {
            auto str = ParseString();
            if (!str.has_value()) {
                return std::nullopt;
            }
            return Value(std::move(*str));
        }
        case 't':
            if (MatchLiteral("true")) {
                return Value(true);
            }
            break;
        case 'f':
            if (MatchLiteral("false")) {
                return Value(false);
            }
            break;
        case 'n':
            if (MatchLiteral("null")) {
                return Value(nullptr);
            }
            break;
        default:
            if ((ch == '-') || (ch >= '0' && ch <= '9')) {
                auto number = ParseNumber();
                if (!number.has_value()) {
                    return std::nullopt;
                }
                return Value(*number);
            }
            break;
    }

    SetError("invalid value");
    return std::nullopt;
}

std::optional<Value> Parser::ParseObject() {
    if (Advance() != '{') {
        SetError("expected object");
        return std::nullopt;
    }

    Value::Object object;
    SkipWhitespace();
    if (!IsAtEnd() && Peek() == '}') {
        Advance();
        return Value(std::move(object));
    }

    while (!IsAtEnd()) {
        auto key = ParseString();
        if (!key.has_value()) {
            return std::nullopt;
        }

        SkipWhitespace();
        if (IsAtEnd() || Advance() != ':') {
            SetError("expected ':' in object");
            return std::nullopt;
        }

        auto value = ParseValue();
        if (!value.has_value()) {
            return std::nullopt;
        }
        object.emplace(std::move(*key), std::move(*value));

        SkipWhitespace();
        if (IsAtEnd()) {
            break;
        }

        const char ch = Advance();
        if (ch == '}') {
            return Value(std::move(object));
        }
        if (ch != ',') {
            SetError("expected ',' or '}' in object");
            return std::nullopt;
        }
        SkipWhitespace();
    }

    SetError("unterminated object");
    return std::nullopt;
}

std::optional<Value> Parser::ParseArray() {
    if (Advance() != '[') {
        SetError("expected array");
        return std::nullopt;
    }

    Value::Array array;
    SkipWhitespace();
    if (!IsAtEnd() && Peek() == ']') {
        Advance();
        return Value(std::move(array));
    }

    while (!IsAtEnd()) {
        auto value = ParseValue();
        if (!value.has_value()) {
            return std::nullopt;
        }
        array.emplace_back(std::move(*value));

        SkipWhitespace();
        if (IsAtEnd()) {
            break;
        }

        const char ch = Advance();
        if (ch == ']') {
            return Value(std::move(array));
        }
        if (ch != ',') {
            SetError("expected ',' or ']' in array");
            return std::nullopt;
        }
        SkipWhitespace();
    }

    SetError("unterminated array");
    return std::nullopt;
}

std::optional<std::string> Parser::ParseString() {
    if (Advance() != '"') {
        SetError("expected string");
        return std::nullopt;
    }

    std::string output;
    while (!IsAtEnd()) {
        const char ch = Advance();
        if (ch == '"') {
            return output;
        }
        if (ch == '\\') {
            if (IsAtEnd()) {
                SetError("unterminated escape");
                return std::nullopt;
            }
            const char escaped = Advance();
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    output.push_back(escaped);
                    break;
                case 'b':
                    output.push_back('\b');
                    break;
                case 'f':
                    output.push_back('\f');
                    break;
                case 'n':
                    output.push_back('\n');
                    break;
                case 'r':
                    output.push_back('\r');
                    break;
                case 't':
                    output.push_back('\t');
                    break;
                case 'u': {
                    if (pos_ + 4 > text_.size()) {
                        SetError("invalid unicode escape");
                        return std::nullopt;
                    }
                    unsigned int codepoint = 0;
                    for (size_t i = 0; i < 4; ++i) {
                        const char hex = text_[pos_ + i];
                        codepoint <<= 4;
                        if (hex >= '0' && hex <= '9') {
                            codepoint |= static_cast<unsigned int>(hex - '0');
                        } else if (hex >= 'a' && hex <= 'f') {
                            codepoint |= static_cast<unsigned int>(hex - 'a' + 10);
                        } else if (hex >= 'A' && hex <= 'F') {
                            codepoint |= static_cast<unsigned int>(hex - 'A' + 10);
                        } else {
                            SetError("invalid unicode escape");
                            return std::nullopt;
                        }
                    }
                    pos_ += 4;

                    if (codepoint <= 0x7F) {
                        output.push_back(static_cast<char>(codepoint));
                    } else if (codepoint <= 0x7FF) {
                        output.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
                        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                    } else {
                        output.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
                        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
                    }
                    break;
                }
                default:
                    SetError("invalid escape sequence");
                    return std::nullopt;
            }
            continue;
        }

        output.push_back(ch);
    }

    SetError("unterminated string");
    return std::nullopt;
}

std::optional<double> Parser::ParseNumber() {
    const size_t start = pos_;
    if (Peek() == '-') {
        Advance();
    }

    while (!IsAtEnd() && Peek() >= '0' && Peek() <= '9') {
        Advance();
    }

    if (!IsAtEnd() && Peek() == '.') {
        Advance();
        while (!IsAtEnd() && Peek() >= '0' && Peek() <= '9') {
            Advance();
        }
    }

    if (!IsAtEnd() && (Peek() == 'e' || Peek() == 'E')) {
        Advance();
        if (!IsAtEnd() && (Peek() == '+' || Peek() == '-')) {
            Advance();
        }
        while (!IsAtEnd() && Peek() >= '0' && Peek() <= '9') {
            Advance();
        }
    }

    const std::string token(text_.substr(start, pos_ - start));
    char* end = nullptr;
    const double value = std::strtod(token.c_str(), &end);
    if (end == token.c_str() || *end != '\0') {
        SetError("invalid number");
        return std::nullopt;
    }
    return value;
}

bool Parser::MatchLiteral(std::string_view literal) {
    if (text_.substr(pos_, literal.size()) != literal) {
        return false;
    }
    pos_ += literal.size();
    return true;
}

void Parser::SkipWhitespace() {
    while (!IsAtEnd()) {
        const char ch = Peek();
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') {
            break;
        }
        ++pos_;
    }
}

bool Parser::IsAtEnd() const {
    return pos_ >= text_.size();
}

char Parser::Peek() const {
    return text_[pos_];
}

char Parser::Advance() {
    return text_[pos_++];
}

void Parser::SetError(std::string message) {
    if (error_.empty()) {
        error_ = std::move(message);
    }
}

}  // namespace jsonlite
