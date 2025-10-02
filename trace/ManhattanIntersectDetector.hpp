#pragma once
#include "public.h"

/**
 * @brief 完整的曼哈顿多边形相交检测器
 * * 该类实现了一个高效的曼哈顿多边形相交检测算法，能够处理各种复杂情况，包括边界点相交、边界线相交以及包含关系。
 */
class ManhattanIntersectDetector {
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

	// 射线法检测点是否在曼哈顿多边形内部或边界上
	static bool pointInManhattanPolygon(int x, int y, const Polygon* poly) {
		const auto& points = poly->vertex;
		int crossings = 0;

		for (size_t i = 0; i < points.size(); ++i) {
			size_t next = (i + 1) % points.size();

			int x1 = points[i].x;
			int y1 = points[i].y;
			int x2 = points[next].x;
			int y2 = points[next].y;
			// 确保坐标有序
			if (x1 > x2 || (x1 == x2 && y1 > y2)) {
				std::swap(x1, x2);
				std::swap(y1, y2);
			}

			// 检查点是否在顶点上
			if (x == x1 && y == y1) return true;

			// 检查点是否在水平边上
			if (y1 == y2 && y == y1 && x1 <= x <= x2) return true;

			// 检查点是否在垂直边上
			if (x1 == x2 && x == x1 && y1 <= y <= y2) return true;

			// 水平向右射线法统计交点
			if (y1 == y2) continue; // 水平边，跳过
			if (x1 < x) continue;   // 射线左边的垂直边，跳过
			if ((y1 <= y && y < y2) || (y2 <= y && y < y1)) { // 如果射线与其右边的垂直边y范围有交集
				crossings++;
			}
		}

		return (crossings % 2) == 1;
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
		const auto& points = poly->vertex;

		for (size_t i = 0; i < points.size(); ++i) {
			size_t next = (i + 1) % points.size();

			int x1 = points[i].x;
			int y1 = points[i].y;
			int x2 = points[next].x;
			int y2 = points[next].y;

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
		const auto& vertex_iter = poly1->vertex.begin();
		int x = poly1->vertex.begin()->x;
		int y = poly1->vertex.begin()->y;

		return pointInManhattanPolygonInterior(x, y, poly2);
	}

	// 射线法检测点是否在曼哈顿多边形内部(只能检测内部的，若点在边界上结果不定)
	static bool pointInManhattanPolygonInterior(int x, int y, const Polygon* poly) {
		const auto& points = poly->vertex;
		int crossings = 0;

		for (size_t i = 0; i < points.size(); ++i) {
			size_t next = (i + 1) % points.size();

			int x1 = points[i].x;
			int y1 = points[i].y;
			int x2 = points[next].x;
			int y2 = points[next].y;

			// 对于曼哈顿多边形，边要么是水平的要么是垂直的
			if (y1 == y2) continue; // 水平边，跳过
			if (x1 < x) continue;   // 射线左边的垂直边，跳过
			if ((y1 <= y && y < y2) || (y2 <= y && y < y1)) { // 如果射线与其右边的垂直边y范围有交集
				crossings++;
			}
		}

		return (crossings % 2) == 1;
	}
};