#pragma once
#include "public.h"
#include "Input.hpp"
#include "SpaceIndex.hpp"
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
 * @brief 完整的曼哈顿多边形相交检测器
 * * 该类实现了一个高效的曼哈顿多边形相交检测算法，能够处理各种复杂情况，包括边界点相交、边界线相交以及包含关系。
 */
class ManhattanCompleteIntersectionDetector {
public:
	// 曼哈顿多边形相交检测，使用特征
	static bool manhattanPolygonsIntersect(const Polygon* poly1, const Polygon* poly2) {
		// 1. 快速边界框检测
		if (!boundingBoxIntersect(poly1, poly2)) {
			return false;
		}

		// 2. 提取多边形的边
		auto edges1 = extractEdges(poly1);
		auto edges2 = extractEdges(poly2);

		// 3. 检测边-边相交（包括边界点相交和边界线相交）
		if (edgesIntersect(edges1, edges2)) {
			return true;
		}

		// 4. 检测包含关系（一个多边形在另一个内部）
		if (polygonContainment(poly1, poly2)) {
			return true;
		}

		return false;
	}

private:
	// 边结构
	struct Edge {
		int x1, y1, x2, y2;
		bool is_horizontal;

		Edge(int _x1, int _y1, int _x2, int _y2)
			: x1(_x1), y1(_y1), x2(_x2), y2(_y2) {
			// 确保坐标有序
			if (x1 > x2 || (x1 == x2 && y1 > y2)) {
				std::swap(x1, x2);
				std::swap(y1, y2);
			}
			is_horizontal = (y1 == y2);
		}
	};

	// 边界框相交检测
	static bool boundingBoxIntersect(const Polygon* poly1, const Polygon* poly2) {
		return poly1->rect.Intersects(poly2->rect);
	}

	// 提取多边形的所有边
	static std::vector<Edge> extractEdges(const Polygon* poly) {
		std::vector<Edge> edges;
		const auto& points = poly->cgal_poly;

		for (size_t i = 0; i < points.size(); ++i) {
			size_t next = (i + 1) % points.size();

			int x1 = static_cast<int>(points[i].x());
			int y1 = static_cast<int>(points[i].y());
			int x2 = static_cast<int>(points[next].x());
			int y2 = static_cast<int>(points[next].y());

			edges.emplace_back(x1, y1, x2, y2);
		}

		return edges;
	}

	// 检测两组边是否相交
	static bool edgesIntersect(const std::vector<Edge>& edges1, const std::vector<Edge>& edges2) {
		for (const auto& edge1 : edges1) {
			for (const auto& edge2 : edges2) {
				if (twoEdgesIntersect(edge1, edge2)) {
					return true;
				}
			}
		}
		return false;
	}

	// 检测两条边是否相交（水平-水平、垂直-垂直、水平-垂直）
	static bool twoEdgesIntersect(const Edge& e1, const Edge& e2) {
		// 情况1：两条都是水平边
		if (e1.is_horizontal && e2.is_horizontal) {
			if (e1.y1 == e2.y1) { // 同一水平线
				// 检测线段重叠
				return !(e1.x2 < e2.x1 || e2.x2 < e1.x1);
			}
			return false;
		}

		// 情况2：两条都是垂直边
		if (!e1.is_horizontal && !e2.is_horizontal) {
			if (e1.x1 == e2.x1) { // 同一垂直线
				// 检测线段重叠
				return !(e1.y2 < e2.y1 || e2.y2 < e1.y1);
			}
			return false;
		}

		// 情况3：一条水平边，一条垂直边
		const Edge* h_edge = e1.is_horizontal ? &e1 : &e2;
		const Edge* v_edge = e1.is_horizontal ? &e2 : &e1;

		// 检测水平边和垂直边的交点
		return (h_edge->x1 <= v_edge->x1 && v_edge->x1 <= h_edge->x2 &&
			v_edge->y1 <= h_edge->y1 && h_edge->y1 <= v_edge->y2);
	}

	// 检测多边形包含关系
	static bool polygonContainment(const Polygon* poly1, const Polygon* poly2) {
		// 检测poly1的任意顶点是否在poly2内部
		if (anyVertexInside(poly1, poly2)) {
			return true;
		}

		// 检测poly2的任意顶点是否在poly1内部
		if (anyVertexInside(poly2, poly1)) {
			return true;
		}

		return false;
	}

	// 检测多边形1的任意一个顶点是否在多边形2内部
	static bool anyVertexInside(const Polygon* poly1, const Polygon* poly2) {
		// 获取第一个顶点进行检测即可
		const auto& vertex_iter = poly1->cgal_poly.vertices_begin();
		int x = static_cast<int>(vertex_iter->x());
		int y = static_cast<int>(vertex_iter->y());

		return pointInManhattanPolygon(x, y, poly2);
	}

	// 射线法检测点是否在曼哈顿多边形内部
	static bool pointInManhattanPolygon(int x, int y, const Polygon* poly) {
		const auto& points = poly->cgal_poly;
		int crossings = 0;

		for (size_t i = 0; i < points.size(); ++i) {
			size_t next = (i + 1) % points.size();

			int x1 = static_cast<int>(points[i].x());
			int y1 = static_cast<int>(points[i].y());
			int x2 = static_cast<int>(points[next].x());
			int y2 = static_cast<int>(points[next].y());

			// 检测水平射线（向右）与边的交点
			if ((y1 <= y && y < y2) || (y2 <= y && y < y1)) {
				// 计算交点的x坐标
				if (y1 == y2) continue; // 水平边，跳过

				// 对于曼哈顿多边形，边要么是水平的要么是垂直的
				if (x1 == x2) { // 垂直边
					if (x1 > x) { // 交点在射线上
						crossings++;
					}
				}
			}
		}

		return (crossings % 2) == 1;
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
				case IntersectionMethod::ORIGINAL_CGAL:
					processWithOriginalCGAL(lfd, edges);
					break;
				case IntersectionMethod::MANHATTAN_COMPLETE:
					processWithManhattanComplete(lfd, edges);
					break;
				case IntersectionMethod::HYBRID_ADAPTIVE:
					processWithHybridAdaptive(lfd, edges);
					break;
				case IntersectionMethod::PARALLEL_CGAL:
					processWithParallelCGAL(lfd, edges);
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
	void processWithOriginalCGAL(const std::vector<Polygon*>& lfd, std::vector<std::pair<int, int>>& edges) {
		stats.cgal_used++;

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
						edges.emplace_back(a->id, b->id);
						stats.intersections_found++;
					}
				}
			}
		}
	}

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
				if (ManhattanCompleteIntersectionDetector::manhattanPolygonsIntersect(a, b)) {
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
			processWithOriginalCGAL(lfd, edges);
		}
		else {
			// 大数据集：使用曼哈顿检测
			processWithManhattanComplete(lfd, edges);
		}
	}

	// 方法4：并行化CGAL
	void processWithParallelCGAL(const std::vector<Polygon*>& lfd, std::vector<std::pair<int, int>>& edges) {
		if (lfd.size() < 100) {
			// 小数据集：避免并行开销
			processWithOriginalCGAL(lfd, edges);
			return;
		}

		stats.cgal_used++;

#pragma omp parallel for schedule(dynamic) if(lfd.size() > 200)
		for (int i = 0; i < (int)lfd.size(); i++) {
			std::vector<std::pair<int, int>> thread_edges;
			Polygon* a = lfd[i];

			for (int j = i + 1; j < (int)lfd.size(); j++) {
				Polygon* b = lfd[j];

				if (a->rect.Intersects(b->rect)) {
					if (CGAL::do_intersect(a->cgal_poly, b->cgal_poly)) {
						thread_edges.emplace_back(a->id, b->id);
					}
				}
			}

#pragma omp critical
			{
				edges.insert(edges.end(), thread_edges.begin(), thread_edges.end());
				stats.intersections_found += thread_edges.size();
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