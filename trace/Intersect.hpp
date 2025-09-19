#pragma once
#include "public.h"
#include "Input.hpp"
#include "SpaceIndex.hpp"


class Intersect {
private:
	Input& input;
	SpaceIndex& spaceIndex;

public:
	Intersect(Input& _input, SpaceIndex& _spaceIndex):input(_input), spaceIndex(_spaceIndex){}

	~Intersect(){}

	// 执行多边形相交检测, 相交视为二者之间存在一条边，获取所有边的集合
	std::vector<Edge> getAllEdge() {
		std::vector<Edge> edges;
		edges.reserve(500000);

		const std::vector<QuadTree*>& quad_trees = spaceIndex.GetSpaceIndex();
		// 对各个四叉树
		for (auto& qtree : quad_trees) {
			// 收集空间索引
			std::vector<std::vector<Polygon*>> leafData;
			std::cout << "Handling QuadTree : " << qtree->_name << std::endl;
			qtree->GetAllLeafData(leafData);

			// 根据空间索引（小格子内），对其内多边形执行 相交检测
			for (auto& lfd : leafData) {
				// n方遍历 内部的多边形对
				for (int i = 0; i < (int)lfd.size(); i++) {
					Polygon* a = lfd[i];
					for (int j = i + 1; j < (int)lfd.size(); j++) {
						Polygon* b = lfd[j];
						//先看矩形框是否相交
						if (a->rect.Intersects(b->rect)) {
							//精细检测是否相交
							//if (CGAL::do_intersect(a->cgal_poly, b->cgal_poly, CGAL::Tag_false())){ // CGAL5.x 则用这个
							if (CGAL::do_intersect(a->cgal_poly, b->cgal_poly)) { // 相交则增加一条边
								edges.emplace_back(Edge(a->id, b->id));
							}
						}
					}
				}
			}
		}
		// TODO 边去重?
		return edges;
	}
};