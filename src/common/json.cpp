#include "json.hpp"
#include <cctype>
#include <stdexcept>
#include <iostream>

namespace auditor {

class JsonParser {
private:
    std::string_view src_;
    size_t pos_ = 0;

    void skip_whitespace() {
        while (pos_ < src_.size() && (src_[pos_] == ' ' || src_[pos_] == '\t' || src_[pos_] == '\r' || src_[pos_] == '\n')) {
            pos_++;
        }
    }

    char peek() {
        skip_whitespace();
        if (pos_ >= src_.size()) return '\0';
        return src_[pos_];
    }

    char get() {
        skip_whitespace();
        if (pos_ >= src_.size()) return '\0';
        return src_[pos_++];
    }

    bool match(std::string_view expected) {
        skip_whitespace();
        if (pos_ + expected.size() <= src_.size() && src_.substr(pos_, expected.size()) == expected) {
            pos_ += expected.size();
            return true;
        }
        return false;
    }

    std::string parse_string() {
        if (get() != '"') {
            return "";
        }
        std::string res;
        while (pos_ < src_.size()) {
            char c = src_[pos_++];
            if (c == '"') {
                return res;
            } else if (c == '\\') {
                if (pos_ >= src_.size()) break;
                char next = src_[pos_++];
                switch (next) {
                    case '"': res += '"'; break;
                    case '\\': res += '\\'; break;
                    case '/': res += '/'; break;
                    case 'b': res += '\b'; break;
                    case 'f': res += '\f'; break;
                    case 'n': res += '\n'; break;
                    case 'r': res += '\r'; break;
                    case 't': res += '\t'; break;
                    case 'u': {
                        if (pos_ + 4 <= src_.size()) {
                            pos_ += 4;
                        }
                        res += '?'; 
                        break;
                    }
                    default: res += next; break;
                }
            } else {
                res += c;
            }
        }
        return res;
    }

    double parse_number() {
        skip_whitespace();
        size_t start = pos_;
        if (pos_ < src_.size() && src_[pos_] == '-') {
            pos_++;
        }
        while (pos_ < src_.size() && (std::isdigit(src_[pos_]) || src_[pos_] == '.' || src_[pos_] == 'e' || src_[pos_] == 'E' || src_[pos_] == '+' || src_[pos_] == '-')) {
            pos_++;
        }
        std::string num_str(src_.substr(start, pos_ - start));
        try {
            return std::stod(num_str);
        } catch (...) {
            return 0.0;
        }
    }

public:
    explicit JsonParser(std::string_view src) : src_(src) {}

    JsonValue parse_value() {
        char c = peek();
        if (c == '\0') {
            return JsonValue(JsonType::Null);
        }

        if (c == '{') {
            return parse_object();
        } else if (c == '[') {
            return parse_array();
        } else if (c == '"') {
            JsonValue val;
            val.type = JsonType::String;
            val.str_val = parse_string();
            return val;
        } else if (c == '-' || std::isdigit(c)) {
            JsonValue val;
            val.type = JsonType::Number;
            val.num_val = parse_number();
            return val;
        } else if (c == 't') {
            if (match("true")) {
                JsonValue val;
                val.type = JsonType::Bool;
                val.bool_val = true;
                return val;
            }
        } else if (c == 'f') {
            if (match("false")) {
                JsonValue val;
                val.type = JsonType::Bool;
                val.bool_val = false;
                return val;
            }
        } else if (c == 'n') {
            if (match("null")) {
                return JsonValue(JsonType::Null);
            }
        }

        if (pos_ < src_.size()) pos_++;
        return JsonValue(JsonType::Null);
    }

    JsonValue parse_object() {
        if (get() != '{') return JsonValue(JsonType::Null);
        JsonValue obj;
        obj.type = JsonType::Object;

        char c = peek();
        if (c == '}') {
            get(); 
            return obj;
        }

        while (true) {
            skip_whitespace();
            if (peek() != '"') {
                break; 
            }
            std::string key = parse_string();
            if (get() != ':') {
                break; 
            }
            JsonValue val = parse_value();
            obj.obj_val[key] = std::move(val);

            char next = peek();
            if (next == ',') {
                get(); 
            } else if (next == '}') {
                get(); 
                break;
            } else {
                break; 
            }
        }
        return obj;
    }

    JsonValue parse_array() {
        if (get() != '[') return JsonValue(JsonType::Null);
        JsonValue arr;
        arr.type = JsonType::Array;

        char c = peek();
        if (c == ']') {
            get(); 
            return arr;
        }

        while (true) {
            arr.arr_val.push_back(parse_value());
            char next = peek();
            if (next == ',') {
                get(); 
            } else if (next == ']') {
                get(); 
                break;
            } else {
                break; 
            }
        }
        return arr;
    }
};

JsonValue parse_json(std::string_view json_str) {
    JsonParser parser(json_str);
    return parser.parse_value();
}

} // namespace auditor
