#pragma once
#include "public.h"
#include "Input.hpp"
#include "Graph.hpp"
#include "QuadTree.hpp"
#include "ManhattanIntersectDetector.hpp"

// 空间索引类定义 - 用于管理多边形数据的空间索引
class SpaceIndex
{
private:
	Input& input;  // 输入数据引用
	Graph graph;  // 图层连通关系图(节点为图层id)
	std::vector<QuadTree*> quad_trees; // 四叉树索引集合
	robin_hood::unordered_map<std::string, QuadTree*> layer_name_to_quadtree; // 图层名到四叉树的映射
	std::vector<std::vector<QuadTree*>> layer_quadtrees; // 每层对应的四叉树索引

public:
	// 构造函数
	SpaceIndex(Input& _input) :input(_input) {
		// 初始化每层的四叉树容器
		layer_quadtrees.resize(input.polygon_id_range_in_layer.size());

		/* 创建图层连通关系图:因为只需要处理存在连通关系的层 */
		CreatLayerGraph();

		/* 创建四叉树空间索引:根据需求分析哪些图层是必要的, 只为必要的图层建立索引 */
		CreatQuadTree();
	}

	// 析构函数 - 清理四叉树内存
	~SpaceIndex() {
		for (auto& ptr : quad_trees)
			if (ptr != nullptr)
				delete ptr;
	}

	// 根据起点位置，获取所在的多边形id，并确保起点在且只在一个多边形内
	int GetStartPosinPolygonId(StartPos& start_pos) {
		int& layer_id = start_pos.first;
		const auto& qtrees = GetQuadTreesForLayer(layer_id);
		assert(!qtrees.empty() && "起点所在层没有空间索引");

		Point& sp = start_pos.second;
		int id = -1;

		// 遍历该层所有四叉树索引
		for (auto* qtree : qtrees) {
			if (!qtree) {
				continue;
			}

			// 获取包含该点的叶子节点中的多边形数据
			const auto* poly_ptr = qtree->GetLeafDataofPoint(sp);
			if (!poly_ptr) {
				continue;
			}

			// 遍历该点所在叶子节点中的所有多边形
			for (auto& pptr : *poly_ptr) {
				if (pptr->layer_id != layer_id) {
					continue;
				}

				// 使用曼哈顿多边形包含性检测
				if (ManhattanIntersectDetector::pointInManhattanPolygon(sp.first, sp.second, pptr)) {
					id = pptr->id;
					break;
				}
			}

			if (id != -1) {
				break;
			}
		}

		assert(id != -1 && "起点不在任何多边形内");
		return id;
	}

	// 获取所有四叉树索引
	const std::vector<QuadTree*>& GetSpaceIndex() const {
		return quad_trees;
	}

	// 根据图层ID获取对应的四叉树索引
	const std::vector<QuadTree*>& GetQuadTreesForLayer(int layer_id) const {
		static const std::vector<QuadTree*> empty;
		if (layer_id < 0 || layer_id >= static_cast<int>(layer_quadtrees.size())) {
			return empty;
		}
		return layer_quadtrees[layer_id];
	}

	// 打印空间索引信息
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
	// 注册图层与四叉树的关联关系
	void RegisterLayerQuadTree(int layer_id, QuadTree* quad_tree) {
		if (!quad_tree) return;
		if (layer_id < 0 || layer_id >= static_cast<int>(layer_quadtrees.size())) return;
		auto& holders = layer_quadtrees[layer_id];

		// 避免重复注册
		for (auto* existing : holders) {
			if (existing == quad_tree) {
				return;
			}
		}
		holders.push_back(quad_tree);
	}

	// 创建图层连通关系图
	void CreatLayerGraph() {
		graph = Graph(static_cast<int>(input.polygon_id_range_in_layer.size()));
		std::vector<Edge> edges;
		edges.reserve(input.via_rules.size());

		// 将所有Via规则作为边添加到图中
		for (auto& via : input.via_rules) {
			edges.push_back(via);
		}
		graph.AddEdges(edges);
	}

	// 根据图层Via关系创建四叉树空间索引
	void CreatQuadTree() {
		std::vector<Polygon*> poly_ptr;
		poly_ptr.reserve(input.total_polygon / 10);

		/* 单起点情况：只为单层建立索引 */
		if (input.start_pos.size() == 1 && input.via_rules.empty()) {
			int layer_id = input.start_pos[0].first;
			std::string& layer_name = input.layer_id_to_name[layer_id];

			poly_ptr.clear();
			Range& a_layer_range = input.polygon_id_range_in_layer[layer_id];

			// 收集该层所有多边形
			for (int j = a_layer_range.first; j <= a_layer_range.second; j++) {
				poly_ptr.push_back(input.polygons[j]);
			}

			// 创建四叉树索引
			QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, layer_name);
			quad_tree->CreatIndex(poly_ptr);
			quad_trees.push_back(quad_tree);
			RegisterLayerQuadTree(layer_id, quad_tree);
			layer_name_to_quadtree[layer_name] = quad_tree;
		}
		/* 单起点多Via：只为起点所在连通分量的层建立索引 */
		else if (input.start_pos.size() == 1 && !input.via_rules.empty()) {
			// 获取起点的连通分量
			int layer_id = input.start_pos[0].first;
			robin_hood::unordered_set<int> concomp = GetConnectComponentofLayer(layer_id);

			// 遍历Via规则，为连通的分量创建合并索引
			for (auto& via : input.via_rules) {
				poly_ptr.clear();
				// 检查Via的两个图层是否都在连通分量中
				if (concomp.find(via.first) != concomp.end() && concomp.find(via.second) != concomp.end()) {
					Range& a_layer_range = input.polygon_id_range_in_layer[via.first];
					for (int i = a_layer_range.first; i <= a_layer_range.second; i++) {
						poly_ptr.push_back(input.polygons[i]);
					}
					Range& b_layer_range = input.polygon_id_range_in_layer[via.second];
					for (int i = b_layer_range.first; i <= b_layer_range.second; i++) {
						poly_ptr.push_back(input.polygons[i]);
					}

					// 创建合并图层的四叉树索引
					std::string name = input.layer_id_to_name[via.first] + "-" + input.layer_id_to_name[via.second];
					QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, name);
					quad_tree->CreatIndex(poly_ptr);
					quad_trees.push_back(quad_tree);

					// 为两个图层都注册这个四叉树索引
					RegisterLayerQuadTree(via.first, quad_tree);
					layer_name_to_quadtree[input.layer_id_to_name[via.first]] = quad_tree;
					RegisterLayerQuadTree(via.second, quad_tree);
					layer_name_to_quadtree[input.layer_id_to_name[via.second]] = quad_tree;
				}
			}
		}
		/* 双起点情况：为两个起点所在的连通分量建立索引 */
		else if (input.start_pos.size() == 2 && !input.via_rules.empty()) {
			// 获取两个起点的连通分量
			int layer_id_s1 = input.start_pos[0].first;
			int layer_id_s2 = input.start_pos[1].first;
			robin_hood::unordered_set<int> concomp_s1 = GetConnectComponentofLayer(layer_id_s1);
			robin_hood::unordered_set<int> concomp_s2 = GetConnectComponentofLayer(layer_id_s2);

			// 遍历Via规则，为属于任一连通分量的Via创建索引
			for (auto& via : input.via_rules) {
				poly_ptr.clear();
				// 检查Via是否属于任一连通分量
				if ((concomp_s1.find(via.first) != concomp_s1.end() && concomp_s1.find(via.second) != concomp_s1.end())
					|| (concomp_s2.find(via.first) != concomp_s2.end() && concomp_s2.find(via.second) != concomp_s2.end())) {
					Range& a_layer_range = input.polygon_id_range_in_layer[via.first];
					for (int i = a_layer_range.first; i <= a_layer_range.second; i++) {
						poly_ptr.push_back(input.polygons[i]);
					}
					Range& b_layer_range = input.polygon_id_range_in_layer[via.second];
					for (int i = b_layer_range.first; i <= b_layer_range.second; i++) {
						poly_ptr.push_back(input.polygons[i]);
					}

					// 创建合并图层的四叉树索引
					std::string name = input.layer_id_to_name[via.first] + "-" + input.layer_id_to_name[via.second];
					QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, name);
					quad_tree->CreatIndex(poly_ptr);
					quad_trees.push_back(quad_tree);

					// 为两个图层都注册这个四叉树索引
					RegisterLayerQuadTree(via.first, quad_tree);
					layer_name_to_quadtree[input.layer_id_to_name[via.first]] = quad_tree;
					RegisterLayerQuadTree(via.second, quad_tree);
					layer_name_to_quadtree[input.layer_id_to_name[via.second]] = quad_tree;
				}
			}
		}
		else throw std::logic_error(__func__ + std::string("未处理的情况"));
	}

	// 获取图层的连通分量
	robin_hood::unordered_set<int> GetConnectComponentofLayer(int layer_id) const {
		std::vector<int> component = graph.GetConnectedComponent(layer_id);
		robin_hood::unordered_set<int> res;
		for (auto& node : component)
			res.insert(node);
		return res;
	}
};