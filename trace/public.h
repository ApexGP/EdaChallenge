#pragma once
#include <vector>
#include <string>
#include <cassert>
#include <numeric>

#include "../third_party/robin_hood/robin_hood.h" // 高效哈希表

#include <CGAL/Simple_cartesian.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Boolean_set_operations_2.h>

const int MAX_DEPTH = 12;     // 四叉树允许的最大深度
const int MAX_DATA_NUM = 10;  // 四叉树预设的单叶子节点内最大数据数量

using Kernel = CGAL::Simple_cartesian<double> ;
using Point_2 = Kernel::Point_2;            //点
using Segment_2 = Kernel::Segment_2;        //线
using Polygon_2 = CGAL::Polygon_2<Kernel>;  //多边形

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
    int id;                      // 多边形id, 0-index
    int layer_id;				 // 所属层id
    Polygon_2 cgal_poly;         // CGAL多边形
    Rect rect;                   // 包络矩形框
};