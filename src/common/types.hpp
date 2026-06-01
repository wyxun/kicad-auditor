#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <ostream>

namespace auditor {

/**
 * @brief 二维几何点结构体，单位默认为毫米 (mm)
 */
struct Point {
    double x = 0.0;
    double y = 0.0;

    // 重载常用运算符
    bool operator==(const Point& other) const = default; // C++20 自动生成比较运算符

    Point operator+(const Point& other) const {
        return {x + other.x, y + other.y};
    }

    Point operator-(const Point& other) const {
        return {x - other.x, y - other.y};
    }

    Point operator*(double scalar) const {
        return {x * scalar, y * scalar};
    }

    // 向量点积
    double dot(const Point& other) const {
        return x * other.x + y * other.y;
    }

    // 向量叉积
    double cross(const Point& other) const {
        return x * other.y - y * other.x;
    }

    // 向量长度（模）的平方
    double length_sq() const {
        return x * x + y * y;
    }

    // 向量长度（模）
    double length() const {
        return std::sqrt(length_sq());
    }

    // 欧氏距离
    double distance_to(const Point& other) const {
        return (*this - other).length();
    }

    /**
     * @brief 计算点到线段 [segment_start, segment_end] 的最短距离
     */
    double distance_to_segment(const Point& segment_start, const Point& segment_end) const {
        const Point ab = segment_end - segment_start;
        const Point ap = *this - segment_start;
        
        const double ab_len_sq = ab.length_sq();
        if (ab_len_sq < 1e-9) {
            // 线段退化为一个点
            return distance_to(segment_start);
        }

        // 计算投影比例 t
        double t = ap.dot(ab) / ab_len_sq;
        // 限制 t 在 [0.0, 1.0] 区间内以保证投影点在线段上
        t = std::clamp(t, 0.0, 1.0);

        // 投影目标点
        const Point projection = segment_start + ab * t;
        return distance_to(projection);
    }
};

// 格式化输出 Point
inline std::ostream& operator<<(std::ostream& os, const Point& p) {
    return os << "(" << p.x << ", " << p.y << ")";
}

/**
 * @brief PCB 导线段/布线段 (Segment)
 */
struct PcbSegment {
    Point start;
    Point end;
    double width = 0.0;     // 线宽
    std::string layer;      // 所在层 (例如 "F.Cu", "B.Cu")
    int net = -1;           // 所属 Net 的数字编号

    /**
     * @brief 计算另一个点到本导线段（考虑线宽）的最短距离
     * 线段带有宽度，所以最近距离是点到线段中心线的距离减去线宽的一半
     */
    double distance_to_point(const Point& p) const {
        double dist = p.distance_to_segment(start, end);
        return std::max(0.0, dist - (width / 2.0));
    }
};

/**
 * @brief PCB 焊盘 (Pad)
 */
struct PcbPad {
    std::string name;             // 焊盘编号/名称，如 "1", "A1"
    std::string type;             // 焊盘类型: "thru_hole" (通孔), "smd" (贴片) 等
    std::string shape;            // 形状: "circle", "rect", "oval", "roundrect" 等
    Point pos;                    // 中心位置
    Point size;                   // 焊盘尺寸 (width, height)
    double drill = 0.0;           // 钻孔尺寸 (仅对 thru_hole 有效)
    std::string layer;            // 所在层
    int net = -1;                 // 所属 Net 编号

    /**
     * @brief 计算点到焊盘边界的最短距离 (做简化估算，支持圆形与矩形)
     */
    double distance_to_point(const Point& p) const {
        if (shape == "circle") {
            double radius = size.x / 2.0;
            double dist = pos.distance_to(p);
            return std::max(0.0, dist - radius);
        } else {
            // 矩形/椭圆等以包围盒 (AABB) 简化计算点到矩形的最近距离
            double half_w = size.x / 2.0;
            double half_h = size.y / 2.0;
            
            double dx = std::max({0.0, std::abs(p.x - pos.x) - half_w});
            double dy = std::max({0.0, std::abs(p.y - pos.y) - half_h});
            return std::sqrt(dx * dx + dy * dy);
        }
    }
};

/**
 * @brief PCB 铺铜区 (Zone)
 */
struct PcbZone {
    int net = -1;                   // 所属 Net 编号
    std::string layer;              // 所在层
    double clearance = 0.0;         // 安全间距 (Clearance) 约束值
    double min_thickness = 0.0;     // 最小敷铜厚度
    std::vector<Point> outline;     // 铺铜区外边界多边形顶点

    /**
     * @brief 计算点到多边形边界的最短距离
     */
    double distance_to_point(const Point& p) const {
        if (outline.empty()) return 1e9;
        
        double min_dist = 1e9;
        size_t n = outline.size();
        for (size_t i = 0; i < n; ++i) {
            const Point& v1 = outline[i];
            const Point& v2 = outline[(i + 1) % n];
            double dist = p.distance_to_segment(v1, v2);
            if (dist < min_dist) {
                min_dist = dist;
            }
        }
        return min_dist;
    }
};

} // namespace auditor
