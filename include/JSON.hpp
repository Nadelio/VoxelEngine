#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace JSON {
    struct Value;

    struct Array {
        std::vector<Value> values;
    };

    struct Object {
        std::vector<std::pair<std::string, Value>> entries;

        const Value* Find(const std::string& key) const;
    };

    using ArrayPtr = std::shared_ptr<Array>;
    using ObjectPtr = std::shared_ptr<Object>;

    struct Value {
        using Variant = std::variant<
            std::monostate,
            bool,
            double,
            std::string,
            ArrayPtr,
            ObjectPtr
        >;

        Variant data;

        Value() = default;
        explicit Value(bool v) : data(v) {}
        explicit Value(double v) : data(v) {}
        explicit Value(std::string v) : data(std::move(v)) {}
        explicit Value(ArrayPtr v) : data(std::move(v)) {}
        explicit Value(ObjectPtr v) : data(std::move(v)) {}

        bool IsNull() const { return std::holds_alternative<std::monostate>(data); }
        bool IsBool() const { return std::holds_alternative<bool>(data); }
        bool IsNumber() const { return std::holds_alternative<double>(data); }
        bool IsString() const { return std::holds_alternative<std::string>(data); }
        bool IsArray() const { return std::holds_alternative<ArrayPtr>(data); }
        bool IsObject() const { return std::holds_alternative<ObjectPtr>(data); }

        bool AsBool() const { return std::get<bool>(data); }
        double AsNumber() const { return std::get<double>(data); }
        const std::string& AsString() const { return std::get<std::string>(data); }
        const Array& AsArray() const { return *std::get<ArrayPtr>(data); }
        const Object& AsObject() const { return *std::get<ObjectPtr>(data); }
    };

    std::optional<Value> ParseString(const std::string& source, std::string* errorMsg = nullptr);
    std::optional<Value> ParseFile(const std::string& path, std::string* errorMsg = nullptr);
}
