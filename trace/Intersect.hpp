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
	MANHATTAN_COMPLETE,          // 曼哈顿多边形检测边
	BATCH_GRID,             	 // 批量打网格检测
	PARALLEL                     // 并行化
};

/**
 * @brief 相交检测统计信息
 */
struct IntersectionStats {
	size_t total_leaf_nodes = 0;
	size_t total_polygon_pairs = 0;
	size_t manhattan_complete_used = 0;
	size_t batch_grid_used = 0;
	size_t prefilter_used = 0;
	size_t intersections_found = 0;
	double total_time_ms = 0.0;

	void reset() {
		total_leaf_nodes = 0;
		total_polygon_pairs = 0;
		manhattan_complete_used = 0;
		batch_grid_used = 0;
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
			std::cout << "  Batch Grid: " << batch_grid_used << " ("
				<< (double)batch_grid_used / total_leaf_nodes * 100 << "%)" << std::endl;
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
	BatchGridIntersectDetector bgid;

	// ========== 配置选项：选择检测相交方法 ==========
	IntersectionMethod method = IntersectionMethod::MANHATTAN_COMPLETE;    // 两两曼哈顿多边形边边相交检测
	 //IntersectionMethod method = IntersectionMethod::BATCH_GRID;           // 批量曼哈顿多边形打网格检测
	// IntersectionMethod method = IntersectionMethod::PARALLEL;             // 并行化

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

		// 根据配置的方法进行处理
		switch (method) {
		case IntersectionMethod::MANHATTAN_COMPLETE:
			processWithManhattanComplete(quad_trees, edges);
			break;
		case IntersectionMethod::BATCH_GRID:
			processWithBatchGrid(quad_trees, edges);
			break;
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
		case IntersectionMethod::MANHATTAN_COMPLETE: return "Manhattan Complete Detection";
		case IntersectionMethod::BATCH_GRID: return "Batch Grid";
		case IntersectionMethod::PARALLEL: return "Parallel";
		default: return "Unknown";
		}
	}

	// 获取性能统计
	const IntersectionStats& getStats() const { return stats; }

private:
	// ========== 各种检测方法的实现 ==========

	// 方法1：两两曼哈顿多边形边边相交检测
	void processWithManhattanComplete(const std::vector<QuadTree*>& quad_trees, std::vector<std::pair<int, int>>& edges) {
		// 对各个四叉树
		std::vector<std::vector<Polygon*>> leafData;
		leafData.reserve(1000000);
		for (auto& qtree : quad_trees) {
			// 收集空间索引
			leafData.clear();
			std::cout << "Handling QuadTree : " << qtree->_name << std::endl;
			qtree->GetAllLeafData(leafData);

			// 根据空间索引（小格子内），对其内多边形执行相交检测
			for (size_t i = 0; i < leafData.size(); ++i) {
				const auto& lfd = leafData[i];
				if (lfd.size() < 2) continue;

				stats.total_leaf_nodes++;
				stats.total_polygon_pairs += (lfd.size() * (lfd.size() - 1)) / 2;
				stats.manhattan_complete_used++;

				// 建立小型并查集
				if (lfd.size() > ufs.getSize())
					ufs = UnionFindSet(lfd.size() * 2);
				else
					ufs.init();

				// n方遍历 内部的多边形对
				for (int i = 0; i < (int)lfd.size(); i++) {
					Polygon* a = lfd[i];
					for (int j = i + 1; j < (int)lfd.size(); j++) {
						Polygon* b = lfd[j];
						if (ufs.find(i) == ufs.find(j)) continue; // 已经连通的跳过
						// 使用曼哈顿多边形相交检测
						if (ManhattanIntersectDetector::manhattanPolygonsIntersect(a, b)) {
							edges.emplace_back(a->id, b->id);
							ufs.join(i, j);
							stats.intersections_found++;
						}
					}
				}
			}
		}
	}

	// 方法2：批量打网格
	void processWithBatchGrid(const std::vector<QuadTree*>& quad_trees, std::vector<std::pair<int, int>>& edges) {
		// 对各个四叉树
		std::vector<std::vector<Polygon*>> leafData;
		std::vector<Rect> leafRect;
		leafData.reserve(1000000);
		leafRect.reserve(1000000);
		for (auto& qtree : quad_trees) {
			// 收集空间索引
			leafData.clear();
			leafRect.clear();
			std::cout << "Handling QuadTree : " << qtree->_name << std::endl;
			qtree->GetAllLeafData(leafData, leafRect);

			// 根据空间索引（小格子内），对其内多边形执行相交检测
			for (size_t i = 0; i < leafData.size(); ++i) {
				const auto& lfd = leafData[i];
				const auto& rect = leafRect[i];
				if (lfd.size() < 2) continue;

				stats.total_leaf_nodes++;
				stats.total_polygon_pairs += (lfd.size() * (lfd.size() - 1)) / 2;
				stats.batch_grid_used++;

				// 使用批量打网格方式
				size_t edges_nums_before = edges.size();
				bgid.batchManhattanPolygonsIntersect(rect, lfd, edges);
				stats.intersections_found += (edges.size() - edges_nums_before);
			}
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