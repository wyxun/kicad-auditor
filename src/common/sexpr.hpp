#pragma once

#include <string>
#include <vector>
#include <memory>
#include <string_view>

namespace auditor {

/**
 * @brief 零第三方大库依赖的 S-Expression 树节点
 */
struct SExpr {
    std::string head;                             // 关键字（首个不带引号的 Atom），如果不存在则为空
    std::string value;                            // 叶子节点内容，若是括号表达式则为空
    std::vector<std::unique_ptr<SExpr>> children; // 子节点指针数组

    // 默认构造函数
    SExpr() = default;

    // 显式深拷贝构造函数
    SExpr(const SExpr& other) {
        head = other.head;
        value = other.value;
        children.reserve(other.children.size());
        for (const auto& child : other.children) {
            if (child) {
                children.push_back(std::make_unique<SExpr>(*child));
            }
        }
    }

    // 显式深拷贝赋值运算符
    SExpr& operator=(const SExpr& other) {
        if (this != &other) {
            head = other.head;
            value = other.value;
            children.clear();
            children.reserve(other.children.size());
            for (const auto& child : other.children) {
                if (child) {
                    children.push_back(std::make_unique<SExpr>(*child));
                }
            }
        }
        return *this;
    }

    // 移动构造函数和赋值运算符
    SExpr(SExpr&& other) noexcept = default;
    SExpr& operator=(SExpr&& other) noexcept = default;

    // 默认析构函数
    ~SExpr() = default;

    /**
     * @brief 判断是否为叶子节点 (Atom)
     */
    bool is_leaf() const { return children.empty() && head.empty(); }

    /**
     * @brief 寻找第一个 head 匹配指定关键字的直接子表达式
     */
    const SExpr* find_child(std::string_view key) const;
    SExpr* find_child(std::string_view key);

    /**
     * @brief 寻找所有 head 匹配指定关键字的直接子表达式
     */
    std::vector<const SExpr*> find_children(std::string_view key) const;

    /**
     * @brief 获取指定索引的子节点的 value 字符串（如果存在且是叶子节点）
     */
    std::string_view get_value_at(size_t idx) const;

    /**
     * @brief 获取指定索引的子节点的 double 值
     */
    double get_double_at(size_t idx, double default_val = 0.0) const;

    /**
     * @brief 获取指定索引的子节点的 int 值
     */
    int get_int_at(size_t idx, int default_val = 0) const;
};

/**
 * @brief 从 S-Expression 文本中极速解析出 SExpr 语法树
 * @param content S-Expression 格式的文本内容
 * @return 根节点指针，若解析失败返回 nullptr
 */
std::unique_ptr<SExpr> parse_sexpr(std::string_view content);

} // namespace auditor
