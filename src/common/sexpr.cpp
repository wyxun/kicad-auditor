#include "sexpr.hpp"
#include <cctype>
#include <stdexcept>

namespace auditor {

namespace {

// 快速跳过空白字符
inline void skip_whitespace(std::string_view& src) {
    while (!src.empty()) {
        char c = src[0];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            src.remove_prefix(1);
        } else {
            break;
        }
    }
}

// 解析 Atom (叶子节点内容，如单词、数值、带引号字符串)
std::unique_ptr<SExpr> parse_atom(std::string_view& src) {
    skip_whitespace(src);
    if (src.empty()) return nullptr;

    auto node = std::make_unique<SExpr>();
    if (src[0] == '"') {
        // 带双引号的 Atom
        src.remove_prefix(1); // 跳过起始引号
        std::string val;
        while (!src.empty()) {
            if (src[0] == '"') {
                src.remove_prefix(1); // 跳过结束引号
                break;
            }
            if (src[0] == '\\' && src.size() > 1) {
                // 处理转义字符
                char next = src[1];
                if (next == '"' || next == '\\') {
                    val.push_back(next);
                } else if (next == 'n') {
                    val.push_back('\n');
                } else if (next == 't') {
                    val.push_back('\t');
                } else {
                    val.push_back('\\');
                    val.push_back(next);
                }
                src.remove_prefix(2);
            } else {
                val.push_back(src[0]);
                src.remove_prefix(1);
            }
        }
        node->value = std::move(val);
    } else {
        // 普通不带引号的 Atom (以空格、括号或引号为分界符)
        size_t len = 0;
        while (len < src.size()) {
            char c = src[len];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
                c == '(' || c == ')' || c == '"') {
                break;
            }
            len++;
        }
        if (len > 0) {
            node->value = std::string(src.substr(0, len));
            src.remove_prefix(len);
        } else {
            return nullptr;
        }
    }
    return node;
}

// 递归下降解析核心逻辑
std::unique_ptr<SExpr> parse_sexpr_impl(std::string_view& src) {
    skip_whitespace(src);
    if (src.empty()) return nullptr;

    if (src[0] == '(') {
        src.remove_prefix(1); // 跳过 '('
        skip_whitespace(src);

        auto node = std::make_unique<SExpr>();

        // 检查第一个元素是否可以作为 head (不带双引号且不是子括号表达式)
        if (!src.empty() && src[0] != ')' && src[0] != '(' && src[0] != '"') {
            auto first = parse_atom(src);
            if (first) {
                node->head = std::move(first->value);
            }
        }

        // 循环解析子节点，直至遇到 ')'
        while (true) {
            skip_whitespace(src);
            if (src.empty()) {
                // 语法不完整/括号不匹配，直接容错退出
                break;
            }
            if (src[0] == ')') {
                src.remove_prefix(1); // 跳过 ')'
                break;
            }

            if (src[0] == '(') {
                auto child = parse_sexpr_impl(src);
                if (child) {
                    node->children.push_back(std::move(child));
                }
            } else {
                auto child = parse_atom(src);
                if (child) {
                    node->children.push_back(std::move(child));
                }
            }
        }
        return node;
    } else {
        // 如果顶层不是以 '(' 开头，则作为一个单独的 Atom 解析
        return parse_atom(src);
    }
}

} // namespace

// 外部解析入口
std::unique_ptr<SExpr> parse_sexpr(std::string_view content) {
    std::string_view src = content;
    return parse_sexpr_impl(src);
}

// ==========================================
// SExpr 成员查询与辅助方法实现
// ==========================================

const SExpr* SExpr::find_child(std::string_view key) const {
    for (const auto& child : children) {
        if (child && child->head == key) {
            return child.get();
        }
    }
    return nullptr;
}

SExpr* SExpr::find_child(std::string_view key) {
    for (auto& child : children) {
        if (child && child->head == key) {
            return child.get();
        }
    }
    return nullptr;
}

std::vector<const SExpr*> SExpr::find_children(std::string_view key) const {
    std::vector<const SExpr*> result;
    for (const auto& child : children) {
        if (child && child->head == key) {
            result.push_back(child.get());
        }
    }
    return result;
}

std::string_view SExpr::get_value_at(size_t idx) const {
    if (idx < children.size() && children[idx]) {
        return children[idx]->value;
    }
    return "";
}

double SExpr::get_double_at(size_t idx, double default_val) const {
    if (idx < children.size() && children[idx]) {
        try {
            return std::stod(std::string(children[idx]->value));
        } catch (...) {
            return default_val;
        }
    }
    return default_val;
}

int SExpr::get_int_at(size_t idx, int default_val) const {
    if (idx < children.size() && children[idx]) {
        try {
            return std::stoi(std::string(children[idx]->value));
        } catch (...) {
            return default_val;
        }
    }
    return default_val;
}

} // namespace auditor
