#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace jsonlite {

class Value {
public:
    using Object = std::map<std::string, Value>;
    using Array = std::vector<Value>;

    Value() = default;
    Value(std::nullptr_t);
    Value(bool value);
    Value(double value);
    Value(std::string value);
    Value(Object value);
    Value(Array value);

    bool IsNull() const;
    bool IsBool() const;
    bool IsNumber() const;
    bool IsString() const;
    bool IsObject() const;
    bool IsArray() const;

    std::optional<bool> AsBool() const;
    std::optional<double> AsNumber() const;
    std::optional<int> AsInt() const;
    std::optional<std::string_view> AsString() const;
    const Object* AsObject() const;
    const Array* AsArray() const;

    const Value* Find(std::string_view key) const;

private:
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Object, Array>;
    Storage storage_ = nullptr;
};

class Parser {
public:
    explicit Parser(std::string_view text);

    std::optional<Value> Parse();
    const std::string& Error() const;

private:
    std::optional<Value> ParseValue();
    std::optional<Value> ParseObject();
    std::optional<Value> ParseArray();
    std::optional<std::string> ParseString();
    std::optional<double> ParseNumber();

    bool MatchLiteral(std::string_view literal);
    void SkipWhitespace();
    bool IsAtEnd() const;
    char Peek() const;
    char Advance();
    void SetError(std::string message);

    std::string_view text_;
    size_t pos_ = 0;
    std::string error_;
};

}  // namespace jsonlite
