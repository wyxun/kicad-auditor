#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>

namespace auditor {

enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

struct JsonValue {
    JsonType type = JsonType::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::unordered_map<std::string, JsonValue> obj_val;

    JsonValue() = default;
    explicit JsonValue(JsonType t) : type(t) {}

    bool is_null() const { return type == JsonType::Null; }
    bool is_bool() const { return type == JsonType::Bool; }
    bool is_number() const { return type == JsonType::Number; }
    bool is_string() const { return type == JsonType::String; }
    bool is_array() const { return type == JsonType::Array; }
    bool is_object() const { return type == JsonType::Object; }

    const JsonValue& operator[](const std::string& key) const {
        static const JsonValue null_val;
        if (type != JsonType::Object) return null_val;
        auto it = obj_val.find(key);
        if (it == obj_val.end()) return null_val;
        return it->second;
    }

    const JsonValue& operator[](size_t idx) const {
        static const JsonValue null_val;
        if (type != JsonType::Array || idx >= arr_val.size()) return null_val;
        return arr_val[idx];
    }
};

/**
 * @brief 解析给定的 JSON 文本
 * @param json_str JSON 格式的字符串视图
 * @return 解析得到的 JsonValue。如果解析失败，会返回 JsonType::Null。
 */
JsonValue parse_json(std::string_view json_str);

} // namespace auditor
