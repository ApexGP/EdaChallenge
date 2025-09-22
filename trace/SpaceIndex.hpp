#pragma once
#include "public.h"
#include "Input.hpp"
#include "Graph.hpp"
#include "QuadTree.hpp"

// 空间索引类定义
class SpaceIndex
{
private:
	Input& input;
	Graph graph;  // 层间连接关系图(节点为层id)
	std::vector<QuadTree*> quad_trees; // 必要层(或合并连通层)的四叉树
	robin_hood::unordered_map<std::string, QuadTree*> layer_name_to_quadtree; // 层名到四叉树的映射

public:
	SpaceIndex(Input& _input) :input(_input) {
		/* 建立层间连接关系图:因为我们只需处理与起点具有联通关系的层 */
		CreatLayerGraph();

		/* 建立四叉树空间索引:会根据输入情况判断哪些索引是必要的, 只为必要的层建立 */
		CreatQuadTree();
	}

	~SpaceIndex() {
		for (auto& ptr : quad_trees)
			if (ptr != nullptr)
				delete ptr;
	}
	
	// 根据起点位置，获取其所在的多边形id, 若有多个也仅返回其中之一
	int GetStartPosinPolygonId(StartPos& start_pos) {
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
		const Point_2 query_point(sp.first, sp.second);
		for (auto& pptr : poly_ptr) {
			// 快速跳过非起点层多边形
			if (pptr->layer_id != layer_id) continue;

			// 精确位置判断
			const auto position = pptr->cgal_poly.bounded_side(query_point);
			if (position != CGAL::ON_UNBOUNDED_SIDE) { // 点不在多边形外部, 找到匹配多边形
				id = pptr->id;
				break;
			}
		}

		assert(id != -1 && "起点不在任何多边形内");
		return id;
	}

	// 返回建立的索引
	const std::vector<QuadTree*>& GetSpaceIndex() const {
		return quad_trees;
	}

	// 打印索引信息
	void PrintSpaceIndexInfo() {
		using std::cout;
		using std::endl;
		cout << "Total QuadTree Num : " << quad_trees.size() << endl;
		for (auto& q_ptr : quad_trees) {
			cout << q_ptr->_name << " CurrMaxDepth:" << q_ptr->_maxCurrDepth 
				 << " CurrLeafMaxDataNum:" << q_ptr->_maxCurrDataNum
				 << " TotalLeafNum:" << q_ptr->GetLeafNum() << endl;
		}
	}

private:
	//建立层间连接关系图
	void CreatLayerGraph() {
		graph = Graph(static_cast<int>(input.polygon_id_range_in_layer.size()));
		std::vector<Edge> edges;
		for (auto& via : input.via_rules) {
			edges.push_back(via);
		}
		graph.AddEdges(edges);
	}

	// 根据层和Via规则，为多边形建立四叉树空间索引
	void CreatQuadTree() {
		std::vector<Polygon*> poly_ptr;
		poly_ptr.reserve(input.total_polygon / 10);

		/* 单起点单层：只为起点层建立索引*/
		if (input.start_pos.size() == 1 && input.via_rules.empty()) {
			int layer_id = input.start_pos[0].first;
			std::string& layer_name = input.layer_id_to_name[layer_id];

			poly_ptr.clear();
			Range& a_layer_range = input.polygon_id_range_in_layer[layer_id];
			for (int j = a_layer_range.first; j <= a_layer_range.second; j++) { // 取a层多边形
				poly_ptr.push_back(input.polygons[j]);
			}

			// 建立四叉树
			QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, layer_name);
			quad_tree->CreatIndex(poly_ptr);
			quad_trees.push_back(quad_tree);
			layer_name_to_quadtree[layer_name] = quad_tree;
		}
		/* 单起点多层：只为与起点层有联通关系的层建立索引*/
		else if (input.start_pos.size() == 1 && !input.via_rules.empty()) {
			// 关于起点层的联通分量
			int layer_id = input.start_pos[0].first;
			robin_hood::unordered_set<int> concomp = GetConnectComponentofLayer(layer_id);

			// 对Via规则连通层, 两层的多边形合并建立一棵树
			for (auto& via : input.via_rules) {
				poly_ptr.clear();
				// 此联通的两层在起点联通分量中
				if (concomp.find(via.first) != concomp.end() && concomp.find(via.second) != concomp.end()) {
					Range& a_layer_range = input.polygon_id_range_in_layer[via.first];
					for (int i = a_layer_range.first; i <= a_layer_range.second; i++) { // 取a层多边形
						poly_ptr.push_back(input.polygons[i]);
					}
					Range& b_layer_range = input.polygon_id_range_in_layer[via.second];
					for (int i = b_layer_range.first; i <= b_layer_range.second; i++) { // 再取b层多边形
						poly_ptr.push_back(input.polygons[i]);
					}

					// 建立四叉树
					std::string name = input.layer_id_to_name[via.first] +"-"+ input.layer_id_to_name[via.second];
					QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, name);
					quad_tree->CreatIndex(poly_ptr);
					quad_trees.push_back(quad_tree);
					//一个层可能指向多棵合并树，这里任意记录一棵即可
					layer_name_to_quadtree[input.layer_id_to_name[via.first]] = quad_tree;
					layer_name_to_quadtree[input.layer_id_to_name[via.second]] = quad_tree;
				}
			}
		}
		/* 双起点多层：只为与起点层有联通关系的层建立索引(两个起点)*/
		else if (input.start_pos.size() == 2 && !input.via_rules.empty()) {
			// 关于起点层的联通分量
			int layer_id_s1 = input.start_pos[0].first;
			int layer_id_s2 = input.start_pos[1].first;
			robin_hood::unordered_set<int> concomp_s1 = GetConnectComponentofLayer(layer_id_s1);
			robin_hood::unordered_set<int> concomp_s2 = GetConnectComponentofLayer(layer_id_s2);

			// 对Via规则连通层, 两层的多边形合并建立一棵树
			for (auto& via : input.via_rules) {
				poly_ptr.clear();
				// 此联通的两层在起点联通分量中
				if ( (concomp_s1.find(via.first) != concomp_s1.end() && concomp_s1.find(via.second) != concomp_s1.end())
					|| (concomp_s2.find(via.first) != concomp_s2.end() && concomp_s2.find(via.second) != concomp_s2.end()) ) {
					Range& a_layer_range = input.polygon_id_range_in_layer[via.first];
					for (int i = a_layer_range.first; i <= a_layer_range.second; i++) { // 取a层多边形
						poly_ptr.push_back(input.polygons[i]);
					}
					Range& b_layer_range = input.polygon_id_range_in_layer[via.second];
					for (int i = b_layer_range.first; i <= b_layer_range.second; i++) { // 再取b层多边形
						poly_ptr.push_back(input.polygons[i]);
					}

					// 建立四叉树
					std::string name = input.layer_id_to_name[via.first] + "-" + input.layer_id_to_name[via.second];
					QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, name);
					quad_tree->CreatIndex(poly_ptr);
					quad_trees.push_back(quad_tree);
					//一个层可能指向多棵合并树，这里任意记录一棵即可
					layer_name_to_quadtree[input.layer_id_to_name[via.first]] = quad_tree;
					layer_name_to_quadtree[input.layer_id_to_name[via.second]] = quad_tree;
				}
			}
		}
		else throw std::logic_error(__func__ + std::string("未定义规则"));
	}

	// 计算层间联通分量
	robin_hood::unordered_set<int> GetConnectComponentofLayer(int layer_id) const {
		std::vector<int> component = graph.GetConnectedComponent(layer_id);
		robin_hood::unordered_set<int> res;
		for (auto& node : component)
			res.insert(node);
		return res;
	}
};