#pragma once
#include "public.h"
#include "Input.hpp"

#include <CGAL/Simple_cartesian.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Boolean_set_operations_2.h>

typedef CGAL::Simple_cartesian<int> Kernel;
typedef Kernel::Point_2				Point_2; //点
typedef Kernel::Segment_2			Segment_2; //线
typedef Kernel::Vector_2			Vector_2; //向量
typedef CGAL::Polygon_2<Kernel>		Polygon_2; //多边形


//转为CGAL多边形
static Polygon_2 to_CGAL_Polygon_2(const std::vector<Point>& vetex) {
	Polygon_2 poly;
	for (auto& p : vetex) {
		poly.push_back(Point_2(p.first, p.second));
	}
	return poly;
}



class Intersect {
private:
	Input& input;

public:
	Intersect(Input& _input):input(_input){}

	// 执行多边形相交检测, 相交视为二者之间存在一条边，获取所有边的集合
	std::vector<Edge> getAllEdge() {
		std::vector<Edge> edges;

		//1.层内多边形相交检测
		for (auto& range : input.polygon_id_range_in_layer) { // 每一层
			// n方遍历
			for (int i = range.first; i <= range.second; i++) {
				Polygon& a = input.polygons[i];
				Polygon_2 a_cgal = to_CGAL_Polygon_2(a.vetex);
				a_cgal = expandOneUnit(a_cgal); // 仅对其中一个多边形外扩一个单位，由此可检测边重合、点重合的情况
				for (int j = i + 1; j <= range.second; j++) {
					Polygon& b = input.polygons[j];
					Polygon_2 b_cgal = to_CGAL_Polygon_2(b.vetex);

					//是否相交
					if (CGAL::do_intersect(a_cgal, b_cgal)) { // 相交则增加一条边
						edges.push_back(Edge(i, j));
					}
				}
			}
		}

		//2.层间多边形相交检测
		for (auto& via : input.via_rules) { // 对每一个Via规则
			Range& a_layer_range = input.polygon_id_range_in_layer[via.first];
			Range& b_layer_range = input.polygon_id_range_in_layer[via.second];

			// n方遍历
			for (int i = a_layer_range.first; i <= a_layer_range.second; i++) { // 遍历a层多边形
				Polygon& a = input.polygons[i];
				Polygon_2 a_cgal = to_CGAL_Polygon_2(a.vetex);
				a_cgal = expandOneUnit(a_cgal); // 仅对其中一个多边形外扩一个单位，由此可检测边重合、点重合的情况
				for (int j = b_layer_range.first; j <= b_layer_range.second; j++) { // 遍历b层多边形
					Polygon& b = input.polygons[j];
					Polygon_2 b_cgal = to_CGAL_Polygon_2(b.vetex);

					//是否相交
					if (CGAL::do_intersect(a_cgal, b_cgal)) { // 相交则增加一条边
						edges.push_back(Edge(i, j));
					}
				}
			}
		}

		return edges;
	}

private:

	// poly一定是曼哈顿多边形，针对曼哈顿多边形，设计该函数进行多边形向外膨胀一个单位操作（TODO:正确性验证？间隔很小的凹口可能导致退化或自交）
	Polygon_2 expandOneUnit(Polygon_2& poly) {
		// 确保多边形是逆时针方向
		if (poly.is_clockwise_oriented()) {
			poly.reverse_orientation();
		}

		Polygon_2 res(poly); // 拷贝
		
		int n = poly.vertices().size();
		for (int i = 0; i < n; ++i) {
			// 逆时针考虑边
			Point_2 start = poly.vertices()[i];
			Point_2 end = poly.vertices()[(i + 1) % n];

			if (start.x() == end.x()) { // 垂直边
				if (start.y() > end.y()) { // 在左侧,边左移
					res.vertex(i) = Point_2(res.vertex(i).x() - 1, res.vertex(i).y());
					res.vertex((i + 1) % n) = Point_2(res.vertex((i + 1) % n).x() - 1, res.vertex((i + 1) % n).y());
				}
				else { // 在右侧,边右移
					res.vertex(i) = Point_2(res.vertex(i).x() + 1, res.vertex(i).y());
					res.vertex((i + 1) % n) = Point_2(res.vertex((i + 1) % n).x() + 1, res.vertex((i + 1) % n).y());
				}
			}
			else { // 水平边
				if (start.x() > end.x()) { // 在上侧,边上移
					res.vertex(i) = Point_2(res.vertex(i).x(), res.vertex(i).y() + 1);
					res.vertex((i + 1) % n) = Point_2(res.vertex((i + 1) % n).x(), res.vertex((i + 1) % n).y() + 1);
				}
				else { // 在下侧,边下移
					res.vertex(i) = Point_2(res.vertex(i).x(), res.vertex(i).y() - 1);
					res.vertex((i + 1) % n) = Point_2(res.vertex((i + 1) % n).x(), res.vertex((i + 1) % n).y() - 1);
				}
			}
		}
		return res;
	}
};