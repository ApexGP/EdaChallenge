#pragma once
#include "public.h"
#include "Input.hpp"
#include "QuadTree.hpp"

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
	std::vector<QuadTree*> quad_trees; // 各层(或合并连通层)的四叉树
	std::unordered_map<std::string, QuadTree*> layer_name_to_quadtree; // 层名到四叉树的映射

public:
	Intersect(Input& _input):input(_input){
		/* 建立四叉树空间索引 */
		CreatQuadTree();
		auto t = layer_name_to_quadtree["M1"];
		std::vector<Rect> rects;
		t->GetAllNodeRect(rects);
		for (auto r : rects) {
			std::cout << "(" << r._xmin << "," << r._ymin << "),";
			std::cout << "(" << r._xmax << "," << r._ymin << "),";
			std::cout << "(" << r._xmax << "," << r._ymax << "),";
			std::cout << "(" << r._xmin << "," << r._ymax << ")\n";
		}
	}

	~Intersect(){
		for (auto& ptr : quad_trees)
			if (ptr != nullptr)
				delete ptr;
	}

	// 执行多边形相交检测, 相交视为二者之间存在一条边，获取所有边的集合
	std::vector<Edge> getAllEdge() {
		std::vector<Edge> edges;

		// 对各个四叉树
		for (auto& qtree : quad_trees) {
			// 收集空间索引
			std::vector<std::vector<Polygon*>> leafData;
			qtree->GetAllLeafData(leafData);

			// 根据空间索引（小格子内），对其内多边形执行 相交检测
			for (auto& lfd : leafData) {
				// n方遍历 内部的多边形对
				for (int i = 0; i < (int)lfd.size(); i++) {
					Polygon& a = *(lfd[i]);
					Polygon_2 a_cgal = to_CGAL_Polygon_2(a.vetex);
					a_cgal = expandOneUnit(a_cgal); // 仅对其中一个多边形外扩一个单位，由此可检测边重合、点重合的情况
					for (int j = i + 1; j < (int)lfd.size(); j++) {
						Polygon& b = *(lfd[j]);
						Polygon_2 b_cgal = to_CGAL_Polygon_2(b.vetex);

						//是否相交
						if (CGAL::do_intersect(a_cgal, b_cgal)) { // 相交则增加一条边
							edges.push_back(Edge(a.id, b.id));
						}
					}
				}
			}
		}
		// TODO 边去重?
		return edges;
		
		/*
		// n平方暴力
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
		*/

		return edges;
	}

	// 根据起点位置，获取其所在的多边形id, 若有多个也仅返回其中之一
	int getStartPosinPolygonId(StartPos& start_pos) {
		// 获取起点层的四叉树
		int& layer_id = start_pos.first;
		std::string& layer_name = input.layer_id_to_name[layer_id];
		QuadTree* qtree = layer_name_to_quadtree[layer_name];
		assert(qtree != nullptr && "起点不在任何层内");
		
		// 获取起点坐标所在索引格子的多边形数据
		Point& sp = start_pos.second;
		std::vector<Polygon*> poly_ptr = qtree->GetLeafDataofPoint(sp);

		// 判断具体在哪个多边形内，有多个则返回第一个
		int id = -1;
		for (auto& pptr : poly_ptr) {
			Polygon_2 poly_cgal = to_CGAL_Polygon_2(pptr->vetex);
			if (layer_id == pptr->layer_id && poly_cgal.bounded_side(Point_2(sp.first, sp.second)) != CGAL::ON_UNBOUNDED_SIDE) { // 同一层且不在多边形外部
				id = pptr->id;
				break;
			}
		}

		assert(id != -1 && "起点不在任何多边形内");
		return id;
	}
private:

	// 根据层和Via规则，为多边形建立四叉树空间索引
	void CreatQuadTree() {
		//1.对Via规则连通层, 两层的多边形合并建立一棵树
		for (auto& via : input.via_rules) { // 对每一个Via规则
			std::vector<Polygon*> poly_ptr;
			poly_ptr.reserve(10000);
			Range& a_layer_range = input.polygon_id_range_in_layer[via.first];
			for (int i = a_layer_range.first; i <= a_layer_range.second; i++) { // 取a层多边形
				poly_ptr.push_back(&input.polygons[i]);
			}
			Range& b_layer_range = input.polygon_id_range_in_layer[via.second];
			for (int i = b_layer_range.first; i <= b_layer_range.second; i++) { // 再取b层多边形
				poly_ptr.push_back(&input.polygons[i]);
			}

			// 建立四叉树
			QuadTree* quad_tree = new QuadTree(input.layout, 12, 8);
			quad_tree->CreatIndex(poly_ptr);
			quad_trees.push_back(quad_tree);
			//一个层可能指向多棵合并树，这里只记录最后一棵即可
			layer_name_to_quadtree[input.layer_id_to_name[via.first]] = quad_tree;
			layer_name_to_quadtree[input.layer_id_to_name[via.second]] = quad_tree;
		}

		//2.对Via规则不涉及的单层建立一棵树
		for (int i = 0; i < (int)input.polygon_id_range_in_layer.size(); i++) {
			std::string layer_name = input.layer_id_to_name[i];
			// 是Via规则不涉及的单层
			if (layer_name_to_quadtree.find(layer_name) == layer_name_to_quadtree.end()) {
				std::vector<Polygon*> poly_ptr;
				poly_ptr.reserve(10000);
				Range& a_layer_range = input.polygon_id_range_in_layer[i];
				for (int j = a_layer_range.first; j <= a_layer_range.second; j++) { // 取a层多边形
					poly_ptr.push_back(&input.polygons[j]);
				}

				// 建立四叉树
				QuadTree* quad_tree = new QuadTree(input.layout, 12, 8);
				quad_tree->CreatIndex(poly_ptr);
				quad_trees.push_back(quad_tree);
				layer_name_to_quadtree[layer_name] = quad_tree;
			}
		}
	}


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