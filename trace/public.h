#pragma once
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <string>
#include <cassert>

using Range = std::pair<int, int>;		 // 区间简记
using Edge = std::pair<int, int>;        // 边: 两个顶点id
using Point = std::pair<int, int>;		 // 点: x,y坐标
using StartPos = std::pair<int, Point>;  // 起点: 层id和坐标

struct Polygon { // 多边形
	int id;                      // 多边形id, 0-index
	int layer_id;				 // 所属层id
	std::vector<Point> vetex;    // 顶点集合
};

