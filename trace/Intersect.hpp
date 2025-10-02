#pragma once
#include "public.h"
#include "Input.hpp"
#include "SpaceIndex.hpp"
#include "ManhattanIntersectDetector.hpp"
#include <algorithm>
#include <chrono>

/**
 * @brief 相交检测方法枚举
 */
enum class IntersectionMethod {
	ORIGINAL_CGAL = 0,           // CGAL方法
	MANHATTAN_COMPLETE,          // 曼哈顿多边形检测边
	HYBRID_ADAPTIVE,             // 自适应混合策略
	PARALLEL_CGAL                // 并行化CGAL
};

/**
 * @brief 相交检测统计信息
 */
struct IntersectionStats {
	size_t total_leaf_nodes = 0;
	size_t total_polygon_pairs = 0;
	size_t manhattan_complete_used = 0;
	size_t cgal_used = 0;
	size_t prefilter_used = 0;
	size_t intersections_found = 0;
	double total_time_ms = 0.0;

	void reset() {
		total_leaf_nodes = 0;
		total_polygon_pairs = 0;
		manhattan_complete_used = 0;
		cgal_used = 0;
		prefilter_used = 0;
		intersections_found = 0;
		total_time_ms = 0.0;
	}

	void print() const {
		std::cout << "\n=== Intersection Detection Statistics ===" << std::endl;
		std::cout << "Total leaf nodes: " << total_leaf_nodes << std::endl;
		std::cout << "Total polygon pairs: " << total_polygon_pairs << std::endl;
		std::cout << "Intersections found: " << intersections_found << std::endl;
		std::cout << "Total time: " << total_time_ms << " ms" << std::endl;

		if (total_leaf_nodes > 0) {
			std::cout << "Algorithm usage:" << std::endl;
			std::cout << "  Manhattan Complete: " << manhattan_complete_used << " ("
				<< (double)manhattan_complete_used / total_leaf_nodes * 100 << "%)" << std::endl;
			std::cout << "  CGAL: " << cgal_used << " ("
				<< (double)cgal_used / total_leaf_nodes * 100 << "%)" << std::endl;
			std::cout << "  Prefilter: " << prefilter_used << " ("
				<< (double)prefilter_used / total_leaf_nodes * 100 << "%)" << std::endl;
		}

		if (total_polygon_pairs > 0 && intersections_found > 0) {
			std::cout << "Intersection rate: " <<
				(double)intersections_found / total_polygon_pairs * 100 << "%" << std::endl;
		}
		std::cout << "=========================================" << std::endl;
	}
};

/**
 * @brief 主要的相交检测类 
 */
class Intersect {
private:
	Input& input;
	SpaceIndex& spaceIndex;
	mutable IntersectionStats stats;
	UnionFindSet ufs;

	// ========== 配置选项：选择检测相交方法 ==========
	IntersectionMethod method = IntersectionMethod::MANHATTAN_COMPLETE;    // 曼哈顿相交检测

	// IntersectionMethod method = IntersectionMethod::ORIGINAL_CGAL;        // CGAL方法
	// IntersectionMethod method = IntersectionMethod::HYBRID_ADAPTIVE;      // 自适应智能选择

	// ================================================

public:
	Intersect(Input& _input, SpaceIndex& _spaceIndex) : input(_input), spaceIndex(_spaceIndex), ufs(50) {}
	~Intersect() {}

	// 执行多边形相交检测，获取所有边的集合(不一定是所有边，因为我们只需要保证联通分量一致即可，成环的边可以不检测)
	std::vector<std::pair<int, int>> getAllEdge() {
		Timer total_timer;
		stats.reset();

		std::vector<std::pair<int, int>> edges;
		edges.reserve(500000);
		const std::vector<QuadTree*>& quad_trees = spaceIndex.GetSpaceIndex();

		std::cout << "Using intersection method: " << getMethodName() << std::endl;

		// 对各个四叉树
		for (auto& qtree : quad_trees) {
			// 收集空间索引
			std::vector<std::vector<Polygon*>> leafData;
			std::cout << "Handling QuadTree : " << qtree->_name << std::endl;
			qtree->GetAllLeafData(leafData);

			// 根据空间索引（小格子内），对其内多边形执行相交检测
			for (auto& lfd : leafData) {
				if (lfd.size() < 2) continue;

				stats.total_leaf_nodes++;
				stats.total_polygon_pairs += (lfd.size() * (lfd.size() - 1)) / 2;

				// 根据配置的方法进行处理
				switch (method) {
				case IntersectionMethod::MANHATTAN_COMPLETE:
					processWithManhattanComplete(lfd, edges);
					break;
				case IntersectionMethod::HYBRID_ADAPTIVE:
					processWithHybridAdaptive(lfd, edges);
					break;
				}
			}
		}

		// 边去重
		removeDuplicateEdges(edges);

		// 打印统计信息
		stats.total_time_ms = total_timer.ElapsedMs();
		stats.print();

		return edges;
	}

	// 运行时切换检测方法
	void setIntersectionMethod(IntersectionMethod new_method) {
		method = new_method;
		std::cout << "Switched to method: " << getMethodName() << std::endl;
	}

	// 获取当前方法名称
	std::string getMethodName() const {
		switch (method) {
		case IntersectionMethod::ORIGINAL_CGAL: return "Original CGAL";
		case IntersectionMethod::MANHATTAN_COMPLETE: return "Manhattan Complete Detection";
		case IntersectionMethod::HYBRID_ADAPTIVE: return "Hybrid Adaptive";
		case IntersectionMethod::PARALLEL_CGAL: return "Parallel CGAL";
		default: return "Unknown";
		}
	}

	// 获取性能统计
	const IntersectionStats& getStats() const { return stats; }

private:
	// ========== 各种检测方法的实现 ==========

	// 方法1：CGAL方法
	//void processWithOriginalCGAL(const std::vector<Polygon*>& lfd, std::vector<std::pair<int, int>>& edges) {
	//	stats.cgal_used++;

	//	// n方遍历 内部的多边形对
	//	for (int i = 0; i < (int)lfd.size(); i++) {
	//		Polygon* a = lfd[i];
	//		for (int j = i + 1; j < (int)lfd.size(); j++) {
	//			Polygon* b = lfd[j];

	//			//先看矩形框是否相交
	//			if (a->rect.Intersects(b->rect)) {
	//				//精细检测是否相交
	//				//if (CGAL::do_intersect(a->cgal_poly, b->cgal_poly, CGAL::Tag_false())){ // CGAL5.x 则用这个
	//				if (CGAL::do_intersect(a->cgal_poly, b->cgal_poly)) { // 相交则增加一条边
	//					edges.emplace_back(a->id, b->id);
	//					stats.intersections_found++;
	//				}
	//			}
	//		}
	//	}
	//}

	// 方法2：曼哈顿多边形相交检测
	void processWithManhattanComplete(const std::vector<Polygon*>& lfd, std::vector<std::pair<int, int>>& edges) {
		stats.manhattan_complete_used++;
		// 建立小型并查集
		if(lfd.size() > ufs.getSize())
			ufs = UnionFindSet(lfd.size() * 2);
		else
			ufs.init();
		// n方遍历 内部的多边形对
		for (int i = 0; i < (int)lfd.size(); i++) {
			Polygon* a = lfd[i];
			for (int j = i + 1; j < (int)lfd.size(); j++) {
				Polygon* b = lfd[j];
				if(ufs.find(i) == ufs.find(j)) continue; // 已经连通的跳过
				// 使用曼哈顿多边形相交检测
				if (ManhattanIntersectDetector::manhattanPolygonsIntersect(a, b)) {
					edges.emplace_back(a->id, b->id);
					ufs.join(i, j);
					stats.intersections_found++;
				}
			}
		}
	}

	// 方法3：自适应混合策略
	void processWithHybridAdaptive(const std::vector<Polygon*>& lfd, std::vector<std::pair<int, int>>& edges) {
		// 根据数据特征智能选择算法
		if (lfd.size() <= 10) {
			// 小数据集：使用原有方法
			//processWithOriginalCGAL(lfd, edges);
		}
		else {
			// 大数据集：使用曼哈顿检测
			processWithManhattanComplete(lfd, edges);
		}
	}

	// 边去重
	void removeDuplicateEdges(std::vector<std::pair<int, int>>& edges) {
		// 标准化边，first < second
		for (auto& edge : edges) {
			if (edge.first > edge.second) {
				std::swap(edge.first, edge.second);
			}
		}

		// 排序并去重
		std::sort(edges.begin(), edges.end());
		edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
	}
};