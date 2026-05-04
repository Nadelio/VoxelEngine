#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace DataFormat {
    /// An unquoted identifier / tag, e.g. GRASS or temperature.
    struct Tag { std::string name; };

    /// An inclusive range whose endpoints are both integers, e.g. 0..100.
    struct IntRange   { int64_t lo, hi; };

    /// An inclusive range whose endpoints are (or include) floats, e.g. 0.0..1.0.
    struct FloatRange { double  lo, hi; };

    struct Value;

    /// A homogeneous, typed array literal, e.g. float[0.5, 1.0, 2.5].
    struct TypedArray {
        enum class ElemType { Int, Float, String, Bool, Tag };
        ElemType                            elemType{};
        std::vector<std::shared_ptr<Value>> elements;
    };

    /// An inline key-value object, e.g. { temp : 0.0..1.0, biome : "plains" }.
    struct Object {
        std::vector<std::pair<std::string, std::shared_ptr<Value>>> entries;

        /// Returns a pointer to the first value with the given key, or nullptr.
        const Value* Get(const std::string& key) const;
    };

    struct Value {
        using Variant = std::variant<
            std::monostate,   /// null / unset sentinel
            int64_t,          /// integer literal
            double,           /// float literal
            bool,             /// boolean  true / false
            std::string,      /// quoted string  "..."
            Tag,              /// unquoted identifier
            IntRange,         /// integer range   n..m
            FloatRange,       /// float range     n..m
            TypedArray,       /// typed array     type[...]
            Object            /// inline object   { key : value, ... }
        >;
        Variant data;

        Value()                       : data(std::monostate{})                      {}
        explicit Value(int64_t v)     : data(std::in_place_type<int64_t>,    v)     {}
        explicit Value(double  v)     : data(std::in_place_type<double>,     v)     {}
        explicit Value(bool    v)     : data(std::in_place_type<bool>,       v)     {}
        explicit Value(std::string v) : data(std::in_place_type<std::string>,std::move(v)) {}
        explicit Value(Tag         v) : data(std::in_place_type<Tag>,        std::move(v)) {}
        explicit Value(IntRange    v) : data(std::in_place_type<IntRange>,   v)     {}
        explicit Value(FloatRange  v) : data(std::in_place_type<FloatRange>, v)     {}
        explicit Value(TypedArray  v) : data(std::in_place_type<TypedArray>, std::move(v)) {}
        explicit Value(Object      v) : data(std::in_place_type<Object>,     std::move(v)) {}

        bool IsNull()       const { return std::holds_alternative<std::monostate>(data); }
        bool IsInt()        const { return std::holds_alternative<int64_t>      (data); }
        bool IsFloat()      const { return std::holds_alternative<double>       (data); }
        bool IsBool()       const { return std::holds_alternative<bool>         (data); }
        bool IsString()     const { return std::holds_alternative<std::string>  (data); }
        bool IsTag()        const { return std::holds_alternative<Tag>          (data); }
        bool IsIntRange()   const { return std::holds_alternative<IntRange>     (data); }
        bool IsFloatRange() const { return std::holds_alternative<FloatRange>   (data); }
        bool IsArray()      const { return std::holds_alternative<TypedArray>   (data); }
        bool IsObject()     const { return std::holds_alternative<Object>       (data); }

        int64_t            AsInt()        const { return std::get<int64_t>    (data); }
        double             AsFloat()      const { return std::get<double>     (data); }
        bool               AsBool()       const { return std::get<bool>       (data); }
        const std::string& AsString()     const { return std::get<std::string>(data); }
        const Tag&         AsTag()        const { return std::get<Tag>        (data); }
        const IntRange&    AsIntRange()   const { return std::get<IntRange>   (data); }
        const FloatRange&  AsFloatRange() const { return std::get<FloatRange> (data); }
        const TypedArray&  AsArray()      const { return std::get<TypedArray> (data); }
        const Object&      AsObject()     const { return std::get<Object>     (data); }
    };

    /// A parsed .data file: an ordered sequence of top-level key-value entries.
    struct Document {
        std::vector<std::pair<std::string, Value>> entries;

        /// Returns a pointer to the first value with the given key, or nullptr.
        const Value* Get(const std::string& key) const;
        bool         Has(const std::string& key) const { return Get(key) != nullptr; }
    };

    /// Parse a .data file from disk into a Document.
    /// Returns nullopt on failure; if errorMsg is non-null it receives a description.
    std::optional<Document> ParseFile  (const std::string& path,
                                        std::string*       errorMsg = nullptr);

    /// Parse a .data document from an in-memory string.
    std::optional<Document> ParseString(const std::string& source,
                                        std::string*       errorMsg = nullptr);
}
