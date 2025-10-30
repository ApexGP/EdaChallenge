#pragma once
#include "public.h"
#include "Input.hpp"
#include "QuadTree.hpp"
#include "ManhattanIntersectDetector.hpp"

//enum SpaceIndexMethod
//{
//	Merged,     // 相连的两层合并建一棵树
//	Separated   // 每层独立建一棵树
//};

// 空间索引类定义 - 用于管理多边形数据的空间索引
class SpaceIndex
{
private:
	Input& input;  // 输入数据引用
	std::vector<QuadTree*> quad_trees; // 四叉树索引集合
	robin_hood::unordered_map<std::string, QuadTree*> layer_name_to_quadtree; // 图层名到四叉树的映射
	std::vector<std::vector<QuadTree*>> layer_quadtrees; // 每层所联通的各层对应的四叉树索引（包括该层本身的树）

public:
	// 构造函数
	explicit SpaceIndex(Input& _input) :input(_input) {}

	// 析构函数 - 清理四叉树内存
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
		const auto* poly_ptr = qtree->GetLeafDataofPoint(sp);

		// 判断具体在哪个多边形内，有多个则返回第一个
		int id = -1;
		for (auto& pptr : *poly_ptr) {
			// 快速跳过非起点层多边形
			if (pptr->layer_id != layer_id) continue;

			// 精确位置判断
			if (ManhattanIntersectDetector::pointInManhattanPolygon(sp.first, sp.second, pptr)) { // 点在多边形内部或边界上，找到了
				id = pptr->id;
				break;
			}
		}

		assert(id != -1 && "起点不在任何多边形内");
		return id;
	}
	// 并行版本
	int GetStartPosinPolygonIdParallel(StartPos& start_pos, int thread_count) {
		// 获取起点层的四叉树
		int& layer_id = start_pos.first;
		std::string& layer_name = input.layer_id_to_name[layer_id];
		QuadTree* qtree = layer_name_to_quadtree[layer_name];
		assert(qtree != nullptr && "起点不在任何层内");

		// 获取起点坐标所在索引格子的多边形数据
		Point& sp = start_pos.second;
		const auto* poly_ptr = qtree->GetLeafDataofPoint(sp);

		// 判断具体在哪个多边形内，有多个则返回第一个
		int id = -1;
		for (auto& pptr : *poly_ptr) {
			// 快速跳过非起点层多边形
			if (pptr->layer_id != layer_id) continue;

			// 精确位置判断
			if (ManhattanIntersectDetector::pointInManhattanPolygon(sp.first, sp.second, pptr)) { // 点在多边形内部或边界上，找到了
				id = pptr->id;
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

	// 根据图层ID获取其关联的四叉树索引
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

	// 根据图层Via关系创建四叉树空间索引-相连的两层合并建一棵树
	void CreatSpaceIndexMerged() {
		/* 单起点单层：只为起点层建立索引 */
		if (input.start_pos.size() == 1 && input.via_rules.empty()) {
			int layer_id = input.start_pos[0].first;
			std::string& layer_name = input.layer_id_to_name[layer_id];

			// 建立四叉树
			QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, layer_name);
			CreatQuadTreeIndex(quad_tree);
			quad_trees.push_back(quad_tree);
			layer_name_to_quadtree[layer_name] = quad_tree;
		}
		/* 单起点多层：只为起点所在连通分量的层建立索引 */
		else if (input.start_pos.size() == 1 && !input.via_rules.empty()) {
			// 关于起点层的联通分量
			int layer_id = input.start_pos[0].first;
			robin_hood::unordered_set<int> concomp = GetConnectComponentofLayer(layer_id);

			// 对Via规则连通层, 两层的多边形合并建立一棵树
			for (auto& via : input.via_rules) {
				// 此联通的两层在起点联通分量中
				if (concomp.find(via.first) != concomp.end() && concomp.find(via.second) != concomp.end()) {
					// 建立四叉树
					std::string name = input.layer_id_to_name[via.first] + "-" + input.layer_id_to_name[via.second];
					QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, name);
					CreatQuadTreeIndex(quad_tree);
					quad_trees.push_back(quad_tree);
					//一个层可能指向多棵合并树，这里任意记录一棵即可
					layer_name_to_quadtree[input.layer_id_to_name[via.first]] = quad_tree;
					layer_name_to_quadtree[input.layer_id_to_name[via.second]] = quad_tree;
				}
			}
		}
		/* 双起点多层：为两个起点所在的连通分量建立索引 */
		else if (input.start_pos.size() == 2 && !input.via_rules.empty()) {
			// 关于起点层的联通分量
			int layer_id_s1 = input.start_pos[0].first;
			int layer_id_s2 = input.start_pos[1].first;
			robin_hood::unordered_set<int> concomp_s1 = GetConnectComponentofLayer(layer_id_s1);
			robin_hood::unordered_set<int> concomp_s2 = GetConnectComponentofLayer(layer_id_s2);

			// 对Via规则连通层, 两层的多边形合并建立一棵树
			for (auto& via : input.via_rules) {
				// 此联通的两层在起点联通分量中
				if ((concomp_s1.find(via.first) != concomp_s1.end() && concomp_s1.find(via.second) != concomp_s1.end())
					|| (concomp_s2.find(via.first) != concomp_s2.end() && concomp_s2.find(via.second) != concomp_s2.end())) {
					// 建立四叉树
					std::string name = input.layer_id_to_name[via.first] + "-" + input.layer_id_to_name[via.second];
					QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, name);
					CreatQuadTreeIndex(quad_tree);
					quad_trees.push_back(quad_tree);
					//一个层可能指向多棵合并树，这里任意记录一棵即可
					layer_name_to_quadtree[input.layer_id_to_name[via.first]] = quad_tree;
					layer_name_to_quadtree[input.layer_id_to_name[via.second]] = quad_tree;
				}
			}
		}
		else throw std::logic_error(__func__ + std::string("未处理的情况"));
	}

	// 根据图层Via关系创建四叉树空间索引-每层独立建一棵树，注册每层联通关系
	void CreatSpaceIndexSeparated() {
		layer_quadtrees.resize(input.polygon_id_range_in_layer.size()); // 初始化每层关联的四叉树容器

		/* 单起点情况：只为起点层建立索引 */
		if (input.start_pos.size() == 1 && input.via_rules.empty()) {
			int layer_id = input.start_pos[0].first;
			std::string& layer_name = input.layer_id_to_name[layer_id];

			// 创建四叉树索引
			QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, layer_name);
			CreatQuadTreeIndex(quad_tree);
			quad_trees.push_back(quad_tree);
			layer_name_to_quadtree[layer_name] = quad_tree;

			// 注册自己的四叉树
			layer_quadtrees[layer_id].push_back(quad_tree);
		}
		/* 单起点多层：只为起点所在连通分量的层建立索引 */
		else if (input.start_pos.size() == 1 && !input.via_rules.empty()) {
			// 获取起点的连通分量
			int layer_id = input.start_pos[0].first;
			robin_hood::unordered_set<int> concomp = GetConnectComponentofLayer(layer_id);

			// 遍历层联通分量，为每层单独建立索引
			for (auto& layer_id : concomp) {
				// 创建该层的四叉树索引
				std::string name = input.layer_id_to_name[layer_id];
				QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, name);
				CreatQuadTreeIndex(quad_tree);
				quad_trees.push_back(quad_tree);
				layer_name_to_quadtree[input.layer_id_to_name[layer_id]] = quad_tree;
				layer_quadtrees[layer_id].push_back(quad_tree);// 注册自己的四叉树
			}

			// 遍历Via规则，注册每层联通的其他四叉树
			for (auto& via : input.via_rules) {
				// 此联通的两层在起点联通分量中
				if (concomp.find(via.first) != concomp.end() && concomp.find(via.second) != concomp.end()) {
					QuadTree* quad_tree1 = layer_name_to_quadtree[input.layer_id_to_name[via.first]];
					QuadTree* quad_tree2 = layer_name_to_quadtree[input.layer_id_to_name[via.second]];
					layer_quadtrees[via.first].push_back(quad_tree2);
					layer_quadtrees[via.second].push_back(quad_tree1);
				}
			}
		}
		/* 双起点多层：为两个起点所在的连通分量建立索引 */
		else if (input.start_pos.size() == 2 && !input.via_rules.empty()) {
			// 获取两个起点的连通分量
			int layer_id_s1 = input.start_pos[0].first;
			int layer_id_s2 = input.start_pos[1].first;
			robin_hood::unordered_set<int> concomp_s1 = GetConnectComponentofLayer(layer_id_s1);
			robin_hood::unordered_set<int> concomp_s2 = GetConnectComponentofLayer(layer_id_s2);

			robin_hood::unordered_set<int> total_concomp;
			for (auto id : concomp_s1) total_concomp.insert(id);
			for (auto id : concomp_s2) total_concomp.insert(id);

			// 遍历层联通分量，为每层单独建立索引
			for (auto& layer_id : total_concomp) {
				// 创建图层的四叉树索引
				std::string name = input.layer_id_to_name[layer_id];
				QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, name);
				CreatQuadTreeIndex(quad_tree);
				quad_trees.push_back(quad_tree);
				layer_name_to_quadtree[input.layer_id_to_name[layer_id]] = quad_tree;
				layer_quadtrees[layer_id].push_back(quad_tree); // 注册自己的四叉树
			}

			// 遍历Via规则，注册每层联通的其他四叉树
			for (auto& via : input.via_rules) {
				// 检查Via是否属于任一连通分量
				if ((concomp_s1.find(via.first) != concomp_s1.end() && concomp_s1.find(via.second) != concomp_s1.end())
					|| (concomp_s2.find(via.first) != concomp_s2.end() && concomp_s2.find(via.second) != concomp_s2.end())) {
					QuadTree* quad_tree1 = layer_name_to_quadtree[input.layer_id_to_name[via.first]];
					QuadTree* quad_tree2 = layer_name_to_quadtree[input.layer_id_to_name[via.second]];
					layer_quadtrees[via.first].push_back(quad_tree2);
					layer_quadtrees[via.second].push_back(quad_tree1);
				}
			}
		}
		else throw std::logic_error(__func__ + std::string("未处理的情况"));
	}

	// 为给定的四叉树执行实际数据划分
	void CreatQuadTreeIndex(QuadTree* quad_tree) {
		std::string& name = quad_tree->_name;
		std::vector<Polygon*> poly_ptr;
		poly_ptr.reserve(input.total_polygon / 10);

		// 分割name确定是单层还是双层合并
		size_t dashPos = name.find('-');
		if (dashPos == std::string::npos) { // 单层
			int layer1_id = input.layer_name_to_id[name];
			// 收集该层所有多边形
			Range& a_layer_range = input.polygon_id_range_in_layer[layer1_id];
			for (int i = a_layer_range.first; i <= a_layer_range.second; i++) { // 取a层多边形
				poly_ptr.push_back(input.polygons[i]);
			}
			// 实际数据划分
			quad_tree->CreatIndex(poly_ptr);
		}
		else{ // 双层合并
			std::string layer1_name = name.substr(0, dashPos);
			std::string layer2_name = name.substr(dashPos + 1);
			int layer1_id = input.layer_name_to_id[layer1_name];
			int layer2_id = input.layer_name_to_id[layer2_name];
			// 收集层的所有多边形
			Range& a_layer_range = input.polygon_id_range_in_layer[layer1_id];
			for (int i = a_layer_range.first; i <= a_layer_range.second; i++) { // 取a层多边形
				poly_ptr.push_back(input.polygons[i]);
			}
			Range& b_layer_range = input.polygon_id_range_in_layer[layer2_id];
			for (int i = b_layer_range.first; i <= b_layer_range.second; i++) { // 再取b层多边形
				poly_ptr.push_back(input.polygons[i]);
			}
			// 实际数据划分
			quad_tree->CreatIndex(poly_ptr);
		}
	}

	// 并行版本：根据图层Via关系创建四叉树空间索引-相连的两层合并建一棵树
	void CreatSpaceIndexMergedParallel(int thread_count) {
		/* 单起点单层：只为起点层建立索引 */
		if (input.start_pos.size() == 1 && input.via_rules.empty()) {
			int layer_id = input.start_pos[0].first;
			std::string& layer_name = input.layer_id_to_name[layer_id];

			// 建立四叉树
			QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, layer_name);
			CreatQuadTreeIndexParallel(quad_tree, thread_count);
			quad_trees.push_back(quad_tree);
			layer_name_to_quadtree[layer_name] = quad_tree;
		}
		/* 单起点多层：只为起点所在连通分量的层建立索引 */
		else if (input.start_pos.size() == 1 && !input.via_rules.empty()) {
			// 关于起点层的联通分量
			int layer_id = input.start_pos[0].first;
			robin_hood::unordered_set<int> concomp = GetConnectComponentofLayer(layer_id);

			// 对Via规则连通层, 两层的多边形合并建立一棵树
			for (auto& via : input.via_rules) {
				// 此联通的两层在起点联通分量中
				if (concomp.find(via.first) != concomp.end() && concomp.find(via.second) != concomp.end()) {
					// 建立四叉树
					std::string name = input.layer_id_to_name[via.first] + "-" + input.layer_id_to_name[via.second];
					QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, name);
					CreatQuadTreeIndexParallel(quad_tree, thread_count);
					quad_trees.push_back(quad_tree);
					//一个层可能指向多棵合并树，这里任意记录一棵即可
					layer_name_to_quadtree[input.layer_id_to_name[via.first]] = quad_tree;
					layer_name_to_quadtree[input.layer_id_to_name[via.second]] = quad_tree;
				}
			}
		}
		/* 双起点多层：为两个起点所在的连通分量建立索引 */
		else if (input.start_pos.size() == 2 && !input.via_rules.empty()) {
			// 关于起点层的联通分量
			int layer_id_s1 = input.start_pos[0].first;
			int layer_id_s2 = input.start_pos[1].first;
			robin_hood::unordered_set<int> concomp_s1 = GetConnectComponentofLayer(layer_id_s1);
			robin_hood::unordered_set<int> concomp_s2 = GetConnectComponentofLayer(layer_id_s2);

			// 对Via规则连通层, 两层的多边形合并建立一棵树
			for (auto& via : input.via_rules) {
				// 此联通的两层在起点联通分量中
				if ((concomp_s1.find(via.first) != concomp_s1.end() && concomp_s1.find(via.second) != concomp_s1.end())
					|| (concomp_s2.find(via.first) != concomp_s2.end() && concomp_s2.find(via.second) != concomp_s2.end())) {
					// 建立四叉树
					std::string name = input.layer_id_to_name[via.first] + "-" + input.layer_id_to_name[via.second];
					QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, name);
					CreatQuadTreeIndexParallel(quad_tree, thread_count);
					quad_trees.push_back(quad_tree);
					//一个层可能指向多棵合并树，这里任意记录一棵即可
					layer_name_to_quadtree[input.layer_id_to_name[via.first]] = quad_tree;
					layer_name_to_quadtree[input.layer_id_to_name[via.second]] = quad_tree;
				}
			}
		}
		else throw std::logic_error(__func__ + std::string("未处理的情况"));
	}

	// 并行版本：根据图层Via关系创建四叉树空间索引-每层独立建一棵树，注册每层联通关系
	void CreatSpaceIndexSeparatedParallel(int thread_count) {
		layer_quadtrees.resize(input.polygon_id_range_in_layer.size()); // 初始化每层关联的四叉树容器

		/* 单起点情况：只为起点层建立索引 */
		if (input.start_pos.size() == 1 && input.via_rules.empty()) {
			int layer_id = input.start_pos[0].first;
			std::string& layer_name = input.layer_id_to_name[layer_id];

			// 创建四叉树索引
			QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, layer_name);
			CreatQuadTreeIndexParallel(quad_tree, thread_count);
			quad_trees.push_back(quad_tree);
			layer_name_to_quadtree[layer_name] = quad_tree;

			// 注册自己的四叉树
			layer_quadtrees[layer_id].push_back(quad_tree);
		}
		/* 单起点多层：只为起点所在连通分量的层建立索引 */
		else if (input.start_pos.size() == 1 && !input.via_rules.empty()) {
			// 获取起点的连通分量
			int layer_id = input.start_pos[0].first;
			robin_hood::unordered_set<int> concomp = GetConnectComponentofLayer(layer_id);

			// 遍历层联通分量，为每层单独建立索引
			for (auto& layer_id : concomp) {
				// 创建该层的四叉树索引
				std::string name = input.layer_id_to_name[layer_id];
				QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, name);
				CreatQuadTreeIndexParallel(quad_tree, thread_count);
				quad_trees.push_back(quad_tree);
				layer_name_to_quadtree[input.layer_id_to_name[layer_id]] = quad_tree;
				layer_quadtrees[layer_id].push_back(quad_tree);// 注册自己的四叉树
			}

			// 遍历Via规则，注册每层联通的其他四叉树
			for (auto& via : input.via_rules) {
				// 此联通的两层在起点联通分量中
				if (concomp.find(via.first) != concomp.end() && concomp.find(via.second) != concomp.end()) {
					QuadTree* quad_tree1 = layer_name_to_quadtree[input.layer_id_to_name[via.first]];
					QuadTree* quad_tree2 = layer_name_to_quadtree[input.layer_id_to_name[via.second]];
					layer_quadtrees[via.first].push_back(quad_tree2);
					layer_quadtrees[via.second].push_back(quad_tree1);
				}
			}
		}
		/* 双起点多层：为两个起点所在的连通分量建立索引 */
		else if (input.start_pos.size() == 2 && !input.via_rules.empty()) {
			// 获取两个起点的连通分量
			int layer_id_s1 = input.start_pos[0].first;
			int layer_id_s2 = input.start_pos[1].first;
			robin_hood::unordered_set<int> concomp_s1 = GetConnectComponentofLayer(layer_id_s1);
			robin_hood::unordered_set<int> concomp_s2 = GetConnectComponentofLayer(layer_id_s2);

			robin_hood::unordered_set<int> total_concomp;
			for (auto id : concomp_s1) total_concomp.insert(id);
			for (auto id : concomp_s2) total_concomp.insert(id);

			// 遍历层联通分量，为每层单独建立索引
			for (auto& layer_id : total_concomp) {
				// 创建图层的四叉树索引
				std::string name = input.layer_id_to_name[layer_id];
				QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, name);
				CreatQuadTreeIndexParallel(quad_tree, thread_count);
				quad_trees.push_back(quad_tree);
				layer_name_to_quadtree[input.layer_id_to_name[layer_id]] = quad_tree;
				layer_quadtrees[layer_id].push_back(quad_tree); // 注册自己的四叉树
			}

			// 遍历Via规则，注册每层联通的其他四叉树
			for (auto& via : input.via_rules) {
				// 检查Via是否属于任一连通分量
				if ((concomp_s1.find(via.first) != concomp_s1.end() && concomp_s1.find(via.second) != concomp_s1.end())
					|| (concomp_s2.find(via.first) != concomp_s2.end() && concomp_s2.find(via.second) != concomp_s2.end())) {
					QuadTree* quad_tree1 = layer_name_to_quadtree[input.layer_id_to_name[via.first]];
					QuadTree* quad_tree2 = layer_name_to_quadtree[input.layer_id_to_name[via.second]];
					layer_quadtrees[via.first].push_back(quad_tree2);
					layer_quadtrees[via.second].push_back(quad_tree1);
				}
			}
		}
		else throw std::logic_error(__func__ + std::string("未处理的情况"));
	}

	// 并行版本：为给定的四叉树执行实际数据划分
	void CreatQuadTreeIndexParallel(QuadTree* quad_tree, int thread_count) {
		std::string& name = quad_tree->_name;
		std::vector<Polygon*> poly_ptr;
		poly_ptr.reserve(input.total_polygon / 10);

		// 分割name确定是单层还是双层合并
		size_t dashPos = name.find('-');
		if (dashPos == std::string::npos) { // 单层
			int layer1_id = input.layer_name_to_id[name];
			// 收集该层所有多边形
			Range& a_layer_range = input.polygon_id_range_in_layer[layer1_id];
			for (int i = a_layer_range.first; i <= a_layer_range.second; i++) { // 取a层多边形
				poly_ptr.push_back(input.polygons[i]);
			}
			// 实际数据划分
			quad_tree->CreatIndexParallel(poly_ptr, thread_count);
		}
		else { // 双层合并
			std::string layer1_name = name.substr(0, dashPos);
			std::string layer2_name = name.substr(dashPos + 1);
			int layer1_id = input.layer_name_to_id[layer1_name];
			int layer2_id = input.layer_name_to_id[layer2_name];
			// 收集层的所有多边形
			Range& a_layer_range = input.polygon_id_range_in_layer[layer1_id];
			for (int i = a_layer_range.first; i <= a_layer_range.second; i++) { // 取a层多边形
				poly_ptr.push_back(input.polygons[i]);
			}
			Range& b_layer_range = input.polygon_id_range_in_layer[layer2_id];
			for (int i = b_layer_range.first; i <= b_layer_range.second; i++) { // 再取b层多边形
				poly_ptr.push_back(input.polygons[i]);
			}
			// 实际数据划分
			quad_tree->CreatIndexParallel(poly_ptr, thread_count);
		}
	}
};