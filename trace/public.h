#pragma once
#include <vector>
#include <string>
#include <cassert>
#include <numeric>
#include <iostream>
#include <fstream>

/* For CGAL if using
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Polygon_set_2.h>
#include <CGAL/Boolean_set_operations_2.h>

using Kernel = CGAL::Simple_cartesian<double>;
using Point_2 = Kernel::Point_2;            //点
using Segment_2 = Kernel::Segment_2;        //线段
using Polygon_2 = CGAL::Polygon_2<Kernel>;  //多边形
using Polygon_with_holes_2 = CGAL::Polygon_with_holes_2<Kernel>;  //带洞多边形
using Polygon_set_2 = CGAL::Polygon_set_2<Kernel>;
*/

#include "../third_party/robin_hood/robin_hood.h" // 高效哈希表
#include "../ManhattanBooleanSetOperation/utils/MPoint_2.h" // 二维点表示
using MPoint_2 = MBSO::MPoint_2;
using Vertexs = std::vector<MPoint_2>;

const int MAX_DEPTH = 12;     // 四叉树最大深度限制
const int MAX_DATA_NUM = 10;  // 四叉树叶子节点最大数据量限制

using Range = std::pair<int, int>;         // 范围区间
using Edge = std::pair<int, int>;          // 边: 多边形id对
using Point = std::pair<int, int>;         // 点: x,y坐标
using StartPos = std::pair<int, Point>;    // 起始位置: 层id和坐标

// 排序边结构
struct SortEdge {
    int x1, y1, x2, y2;
    bool is_horizontal; // 是否水平边

    SortEdge(int _x1, int _y1, int _x2, int _y2)
        : x1(_x1), y1(_y1), x2(_x2), y2(_y2) {
        // 确保起点在终点左边
        if (x1 > x2 || (x1 == x2 && y1 > y2)) {
            std::swap(x1, x2);
            std::swap(y1, y2);
        }
        is_horizontal = (y1 == y2);
    }
};

// 网格边结构
struct GridEdge : public SortEdge {
    int id; // 唯一标识
    int polygon_index; // 所属多边形的索引下标

    GridEdge(int _x1, int _y1, int _x2, int _y2, int _id, int _polygon_index)
        : SortEdge(_x1, _y1, _x2, _y2), id(_id), polygon_index(_polygon_index) {
    }
};

// 矩形结构
struct Rect
{
    // 矩形范围的四个边界值
    int _xmin;
    int _ymin;
    int _xmax;
    int _ymax;

    Rect() : _xmin(INT_MAX), _ymin(INT_MAX), _xmax(INT_MIN), _ymax(INT_MIN) {}

    explicit Rect(int xmin, int ymin, int xmax, int ymax) : _xmin(xmin), _ymin(ymin), _xmax(xmax), _ymax(ymax) {}

    // 重置包围盒
    void reset()
    {
        _xmin = _ymin = INT_MAX;
        _xmax = _ymax = INT_MIN;
    }

    // 使用包围盒更新当前范围(只用于扩大包围盒)
    void update(const Rect& p)
    {
        _xmin = std::min(_xmin, p._xmin);
        _ymin = std::min(_ymin, p._ymin);
        _xmax = std::max(_xmax, p._xmax);
        _ymax = std::max(_ymax, p._ymax);
    }

    // 判断点是否在矩形范围内,包含边界
    bool Contains(int x, int y) const
    {
        return (x >= _xmin) && (x <= _xmax) && (y >= _ymin) && (y <= _ymax);
    }

    // 判断两个矩形是否相交
    bool Intersects(const Rect& other) const {
        return !(other._xmax < _xmin || other._xmin > _xmax ||
            other._ymax < _ymin || other._ymin > _ymax);
    }
};

// 多边形结构
struct Polygon {
    int id = -1;                    // 多边形id, 0-index
    int layer_id = -1;              // 图层id
    Vertexs vertex;                 // 多边形顶点集
    Rect rect;                      // 多边形包围盒
};

#include <chrono>

// 计时器类
class Timer {
public:
    // 构造函数，可选是否立即开始计时
    Timer(bool startImmediately = true) {
        if (startImmediately) {
            Reset();
        }
    }

    // 重置计时器，将开始时间设为当前时间
    void Reset() {
        m_startTime = std::chrono::steady_clock::now();
        m_LastCallTime = m_startTime;
    }

    void LastCallReset() {
        m_LastCallTime = std::chrono::steady_clock::now();
    }

    // 获取从上一次 Reset() 调用后经过的时间（默认返回秒，以 double 表示）
    template<typename Duration = std::chrono::duration<double>>
    typename Duration::rep Elapsed() const {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedDuration = std::chrono::duration_cast<Duration>(currentTime - m_startTime);
        return elapsedDuration.count();
    }

    // 获取从上一次 Reset() 调用后经过的时间（返回毫秒）
    long long ElapsedMs() const {
        return Elapsed<std::chrono::milliseconds>();
    }

    // 获取从上一次 FromLastCallElapsed() 或 FromLastCallElapsedMs() 调用后经过的时间（默认返回秒，以 double 表示）
    template<typename Duration = std::chrono::duration<double>>
    typename Duration::rep FromLastCallElapsed() {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedDuration = std::chrono::duration_cast<Duration>(currentTime - m_LastCallTime);
        LastCallReset();
        return elapsedDuration.count();
    }

    // 获取从上一次 FromLastCallElapsed() 或 FromLastCallElapsedMs() 调用后经过的时间（返回毫秒）
    long long FromLastCallElapsedMs() {
        return FromLastCallElapsed<std::chrono::milliseconds>();
    }

private:
    std::chrono::steady_clock::time_point m_startTime; // 记录开始时间
    std::chrono::steady_clock::time_point m_LastCallTime;  // 记录上一次调用FromLastCallElapsed方法的时间
};

// 并查集类定义
class UnionFindSet {
private:
    std::vector<int> parent;
    int n;

public:
    // 构造函数，初始化并查集
    UnionFindSet(int size) : n(size) {
        parent.resize(n);
        for (int i = 0; i < n; i++) {
            parent[i] = i;
        }
    }

    // 重新初始化
    void init() {
        for (int i = 0; i < n; i++) {
            parent[i] = i;
        }
    }

    int getSize() { return n; }

    // 查找操作，带路径压缩
    int find(int x) {
        if (parent[x] != x) {
            parent[x] = find(parent[x]);  // 路径压缩
        }
        return parent[x];
    }

    // 合并操作
    void join(int x, int y) {
        int rootX = find(x);
        int rootY = find(y);
        if (rootX == rootY) return;

        // 小树合并到大树
        if (rootX < rootY) {
            parent[rootX] = rootY;
        }
        else {
            parent[rootY] = rootX;
        }
    }

    // 获取所有连通分量
    std::vector<std::vector<int>> getComponents() {
        // 预分配内存
        std::vector<int> roots(n);
        std::vector<int> rootMap(n, -1); // 初始化为-1
        std::vector<std::vector<int>> components;
        components.reserve(n); // 预分配最大空间

        // 步骤1: 确定所有根节点，同时路径压缩
        for (int i = 0; i < n; i++) {
            roots[i] = find(i);
        }

        // 步骤2: 遍历分类
        for (int i = 0; i < n; i++) {
            int root = roots[i];

            if (rootMap[root] == -1) {
                // 新建连通分量
                rootMap[root] = components.size();
                components.push_back({ i }); // 直接构造包含当前元素的vector
            }
            else {
                // 添加到已有连通分量
                components[rootMap[root]].push_back(i);
            }
        }

        return components;
    }
};

// util 常规输出多边形函数
static void OutputPolygon(std::ofstream& res_file, Polygon& p) {
    const auto vertex_count = p.vertex.size();
    std::string buffer;
    buffer.reserve(vertex_count * 15);
    for (size_t j = 0; j < vertex_count; ++j) {
        buffer += "(";
        buffer += std::to_string(p.vertex[j].x);
        buffer += ",";
        buffer += std::to_string(p.vertex[j].y);
        buffer += ")";
        if (j + 1 != vertex_count) {
            buffer += ",";
        }
    }
    buffer += "\n";
    res_file << buffer;
}