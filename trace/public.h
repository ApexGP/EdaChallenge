#pragma once
#include <vector>
#include <string>
#include <cassert>
#include <numeric>

#include "../third_party/robin_hood/robin_hood.h" // 高效哈希表

#include <CGAL/Simple_cartesian.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Polygon_set_2.h>
#include <CGAL/Boolean_set_operations_2.h>

const int MAX_DEPTH = 12;     // 四叉树允许的最大深度
const int MAX_DATA_NUM = 10;  // 四叉树预设的单叶子节点内最大数据数量

using Kernel = CGAL::Simple_cartesian<double> ;
using Point_2 = Kernel::Point_2;            //点
using Segment_2 = Kernel::Segment_2;        //线
using Polygon_2 = CGAL::Polygon_2<Kernel>;  //多边形
using Polygon_with_holes_2 = CGAL::Polygon_with_holes_2<Kernel>;  //多边形
using Polygon_set_2 = CGAL::Polygon_set_2<Kernel>;

using Range = std::pair<int, int>;		 // 区间简记
using Edge = std::pair<int, int>;        // 边: 两个顶点id
using Point = std::pair<int, int>;		 // 点: x,y坐标
using StartPos = std::pair<int, Point>;  // 起点: 层id和坐标

// 添加哈希特化
//namespace std {
//    template <>
//    struct hash<std::pair<int, int>> {
//        std::size_t operator()(const std::pair<int, int>& edge) const {
//            std::size_t h1 = robin_hood::hash<int>()(edge.first);
//            std::size_t h2 = robin_hood::hash<int>()(edge.second);
//            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
//        }
//    };
//}

// 定义矩形
struct Rect
{
    // 矩形范围的四个坐标值
    int _xmin;
    int _ymin;
    int _xmax;
    int _ymax;

    Rect() : _xmin(0), _ymin(0), _xmax(0), _ymax(0) {}

    explicit Rect(int xmin, int ymin, int xmax, int ymax) : _xmin(xmin), _ymin(ymin), _xmax(xmax), _ymax(ymax) {}

    // 检查点是否在矩形范围内,包括边界
    bool Contains(int x, int y) const
    {
        return (x >= _xmin) && (x <= _xmax) && (y >= _ymin) && (y <= _ymax);
    }

    // 检查两个矩形是否相交
    bool Intersects(const Rect& other) const {
        return !(other._xmax < _xmin || other._xmin > _xmax ||
            other._ymax < _ymin || other._ymin > _ymax);
    }

    // 获取左上角拆分矩形
    Rect GetLTRect() const
    {
        int xmid = (_xmin + _xmax) / 2;
        int ymid = (_ymin + _ymax) / 2;
        return Rect(_xmin, ymid, xmid, _ymax);
    }

    // 获取右上角拆分矩形  
    Rect GetRTRect() const
    {
        int xmid = (_xmin + _xmax) / 2;
        int ymid = (_ymin + _ymax) / 2;
        return Rect(xmid, ymid, _xmax, _ymax);
    }

    // 获取左下角拆分矩形
    Rect GetLBRect() const
    {
        int xmid = (_xmin + _xmax) / 2;
        int ymid = (_ymin + _ymax) / 2;
        return Rect(_xmin, _ymin, xmid, ymid);
    }

    // 获取右下角拆分矩形
    Rect GetRBRect() const
    {
        int xmid = (_xmin + _xmax) / 2;
        int ymid = (_ymin + _ymax) / 2;
        return Rect(xmid, _ymin, _xmax, ymid);
    }
};

// 定义多边形
struct Polygon {
    int id = -1;                      // 多边形id, 0-index
    int layer_id = -1;				 // 所属层id
    Polygon_2 cgal_poly;         // CGAL多边形
    Rect rect;                   // 包络矩形框
};

#include <chrono>

// 定义计时器类
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

    // 获取自上次 Reset() 调用以来经过的时间（默认返回秒，以 double 表示）
    template<typename Duration = std::chrono::duration<double>>
    typename Duration::rep Elapsed() const {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedDuration = std::chrono::duration_cast<Duration>(currentTime - m_startTime);
        return elapsedDuration.count();
    }
    // 获取自上次 Reset() 调用以来经过的时间（返回毫秒）
    long long ElapsedMs() const {
        return Elapsed<std::chrono::milliseconds>();
    }

    // 获取自上次 FromLastCallElapsed() 或 FromLastCallElapsedMs() 调用以来经过的时间（默认返回秒，以 double 表示）
    template<typename Duration = std::chrono::duration<double>>
    typename Duration::rep FromLastCallElapsed() {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedDuration = std::chrono::duration_cast<Duration>(currentTime - m_LastCallTime);
        LastCallReset();
        return elapsedDuration.count();
    }
    // 获取自上次 FromLastCallElapsed() 或 FromLastCallElapsedMs() 调用以来经过的时间（返回毫秒）
    long long FromLastCallElapsedMs() {
        return FromLastCallElapsed<std::chrono::milliseconds>();
    }

private:
    std::chrono::steady_clock::time_point m_startTime; // 记录开始时间
	std::chrono::steady_clock::time_point m_LastCallTime;  // 记录上次调用FromLastCallElapsed函数的时间
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

	// 查找操作（带路径压缩）
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

		// 按大小合并
		if (rootX < rootY) {
			parent[rootX] = rootY;
		}
		else {
			parent[rootY] = rootX;
		}
	}

	// 获取所有连通分量
	std::vector<std::vector<int>> getComponents() {
		// 确保所有路径都已压缩
		std::vector<int> roots(n);
		for (int i = 0; i < n; i++) {
			roots[i] = find(i);
		}

		// 创建根节点到组件的映射
		robin_hood::unordered_map<int, std::vector<int>> componentsMap;
		for (int i = 0; i < n; i++) {
			componentsMap[roots[i]].push_back(i);
		}

		// 将映射转换为向量
		std::vector<std::vector<int>> components;
		for (auto& pair : componentsMap) {
			components.push_back(pair.second);
		}

		return components;
	}
};
