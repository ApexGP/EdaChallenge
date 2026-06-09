#pragma once
#include "public.h"
#include "./ManhattanBooleanSetOperation/utils/Grid.h"
#include <cmath>
#include <algorithm>

/**
 * @brief 基于n平方边边相交检测和点包含测试的曼哈顿多边形相交检测器
 * * 该类实现了一个高效的曼哈顿多边形相交检测算法，能够处理各种复杂情况，包括边界点相交、边界线相交以及包含关系。
 */
class ManhattanIntersectDetector {
public:
	// 两个曼哈顿多边形相交检测，使用特征
	static bool manhattanPolygonsIntersect(const Polygon* poly1, const Polygon* poly2) {
		// 1. 快速边界框检测
		if (!boundingBoxIntersect(poly1, poly2)) {
			return false;
		}

		// 2. 检测边-边相交（包括边界点相交和边界线相交）
		if (edgesIntersect(poly1, poly2)) {
			return true;
		}

		// 3. 检测包含关系（一个多边形在另一个内部）
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
	// 边界框相交检测
	static bool boundingBoxIntersect(const Polygon* poly1, const Polygon* poly2) {
		return poly1->rect.Intersects(poly2->rect);
	}

	// 检测两组边是否相交
	static bool edgesIntersect(const Polygon* poly1, const Polygon* poly2) {
		// 预先提取多边形2的边
		static thread_local std::vector<SortEdge> edges2;
		edges2.reserve(poly2->vertex.size());
		extractEdges(poly2, edges2);

		// 遍历点建立多边形1的边并检测
		const auto& points1 = poly1->vertex;
		for (size_t i = 0; i < points1.size(); ++i) {
			size_t next = i + 1;
			if(next == points1.size()) next = 0;
			int x1 = points1[i].x;
			int y1 = points1[i].y;
			int x2 = points1[next].x;
			int y2 = points1[next].y;
			// 多边形1的边与多边形2的包围盒直接特判一下
			if (y1 == y2 && (y1 < poly2->rect._ymin || y1 > poly2->rect._ymax)
				|| x1 == x2 && (x1 < poly2->rect._xmin || x1 > poly2->rect._xmax)) continue;
			// 生成多边形1的边
			SortEdge edge1(x1, y1, x2, y2);
			// 再遍历多边形2的边，检测是否相交
			for (const auto& edge2 : edges2) {
				if (twoEdgesIntersect(edge1, edge2)) {
					return true;
				}
			}
		}
		return false;
	}

	// 提取多边形的所有边
	static void extractEdges(const Polygon* poly, std::vector<SortEdge>& edges) {
		edges.clear();
		const auto &points = poly->vertex;

		for (size_t i = 0; i < points.size(); ++i) {
			size_t next = i + 1;
			if(next == points.size()) next = 0;

			int x1 = points[i].x;
			int y1 = points[i].y;
			int x2 = points[next].x;
			int y2 = points[next].y;

			edges.emplace_back(x1, y1, x2, y2);
		}
	}

public:
	// 检测两条边是否相交（水平-水平、垂直-垂直、水平-垂直）
	static bool twoEdgesIntersect(const SortEdge& e1, const SortEdge& e2) {
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
		const SortEdge* h_edge = e1.is_horizontal ? &e1 : &e2;
		const SortEdge* v_edge = e1.is_horizontal ? &e2 : &e1;

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
			size_t next = i + 1;
			if(next == points.size()) next = 0;

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

/**
 * @brief 基于打网格的批量曼哈顿多边形相交检测器
 * * 该类实现了一个高效的批量曼哈顿多边形相交检测算法，能够处理各种复杂情况，包括边界点相交、边界线相交以及包含关系。
 */
class BatchGridIntersectDetector {
private:
	Rect box;				//所有多边形集整体包围盒
	double avgLength;       //边集平均边长

	using Grid = MBSO::Grid<GridEdge*>;
	Grid grid;				//网格存边
	double blockWidth;		//当前每个小网格宽度
	double blockHeight;		//当前每个小网格高度
	int blockCount;		    //当前划分的一行或一列的网格数量，行和列数是相等的

	std::vector<GridEdge> gridEdges; // 存所有的边

	std::vector<bool> calculated;	 // 记录已计算过的边组合(一维数组充当二维)
	UnionFindSet ufs;				 // 并查集，记录已经相交的多边形

public:
	BatchGridIntersectDetector() : avgLength(0), grid(101, 101), blockWidth(0), blockHeight(0), blockCount(0), ufs(50){
		gridEdges.reserve(500);
		calculated.reserve(500 * 500);
	}
	void batchManhattanPolygonsIntersect(const Rect& rect, const std::vector<Polygon*>& polygons, std::vector<std::pair<int, int>>& edges) {
		// 建立小型并查集
		if (polygons.size() > ufs.getSize())
			ufs = UnionFindSet(polygons.size() * 2);
		else
			ufs.init();

		// 初始化
		initial(rect, polygons);

		// 自适应网格
		chooseBlkCntBasedOnBoxRegion();

		// 遍历边集，找出所有边经过的格子
		for (auto& edge : gridEdges) {
			findEdgePassedBlockOnBoxRegion(&edge);
		}

		// 遍历所有的格子，然后遍历格子里面的所有线段
		for (int i = 0; i <= blockCount; ++i)
		{
			for (int j = 0; j <= blockCount; ++j)
			{
				if (grid.grid[i][j].size() <= 1) continue;
				int n = grid.grid[i][j].size();
				for (int ii = 0; ii < n; ++ii)
				{
					auto& edge1 = grid.grid[i][j][ii];
					for (int jj = ii + 1; jj < n; ++jj)
					{
						auto& edge2 = grid.grid[i][j][jj];
						if (ufs.find(edge1->polygon_index) == ufs.find(edge2->polygon_index)) continue; // 已经连通的跳过(同属于一个多边形集的边也会被筛掉)
						//if (calculated[edge1->id * gridEdges.size() + edge2->id]) continue;
						//calculated[edge1->id * gridEdges.size() + edge2->id] = true;
						//calculated[edge2->id * gridEdges.size() + edge1->id] = true;

						// 同一网格内的两条边检测是否有交点,若有则对应的多边形相交
						if (ManhattanIntersectDetector::twoEdgesIntersect(*edge1, *edge2))
						{
							edges.emplace_back(polygons[edge1->polygon_index]->id, polygons[edge2->polygon_index]->id);
							ufs.join(edge1->polygon_index, edge2->polygon_index);
						}
					}
				}
			}
		}

		// 处理嵌套的相交关系
		for (int i = 0; i < (int)polygons.size(); i++) {
			Polygon* a = polygons[i];
			for (int j = i + 1; j < (int)polygons.size(); j++) {
				Polygon* b = polygons[j];
				if (ufs.find(i) == ufs.find(j)) continue; // 已经连通的跳过
				if (!a->rect.Intersects(b->rect)) continue; // 包围盒排除不包含
				// 包含关系检测
				if (ManhattanIntersectDetector::polygonContainment(a, b)) {
					edges.emplace_back(a->id, b->id);
					ufs.join(i, j);
				}
			}
		}
	}

private:
	// 初始化成员
	void initial(const Rect& rect, const std::vector<Polygon*>& polygons) {
		gridEdges.clear();
		gridEdges.reserve(polygons.size() * 20);

		// 输入多边形集的边信息、包围盒初始化
		box = rect;
		avgLength = 0;
		int edge_id = 0;
		int polygon_index = 0;
		for (const auto& poly_ptr : polygons) {
			const auto& points = poly_ptr->vertex;
			for (size_t i = 0; i < points.size(); ++i) {
				size_t next = (i + 1) % points.size();
				int x1 = points[i].x;
				int y1 = points[i].y;
				int x2 = points[next].x;
				int y2 = points[next].y;
				gridEdges.emplace_back(x1, y1, x2, y2, edge_id, polygon_index);
				edge_id++;
				avgLength += (x1 == x2) ? abs(y1 - y2) : abs(x1 - x2);
			}
			polygon_index++;
		}
		avgLength = avgLength / edge_id;
		//calculated.resize(edge_id * edge_id);
		//std::fill(calculated.begin(), calculated.end(), false);

		// 重置网格状态
		grid.clear();
		blockWidth = 0;
		blockHeight = 0;
		blockCount = 0;
	}

	// 在整体包围盒区域自适应打网格
	void chooseBlkCntBasedOnBoxRegion() {
		// 自适应网格
		int t = ceil((box._xmax - box._xmin) / avgLength);
		blockCount = std::min(std::max(t, 5), 100);
		blockHeight = static_cast<double>(box._ymax - box._ymin) / blockCount;
		blockWidth = static_cast<double>(box._xmax - box._xmin) / blockCount;
	}

	//找出该边经过的所有包围盒区域内的网格
	void findEdgePassedBlockOnBoxRegion(GridEdge* v) {
		// 曼哈顿边必然是水平或垂直的
		if (v->y1 == v->y2) { // 水平边：Y固定，遍历X方向网格
			int xmin = v->x1, xmax = v->x2;

			if (v->y1 < box._ymin || v->y1 > box._ymax) return; // 包围盒区域外的边
			int blockYId = getBlockYId(v->y1);
			int startBlockXId = getBlockXId(xmin);
			int endBlockXId = getBlockXId(xmax);
			for (int xId = startBlockXId; xId <= endBlockXId; ++xId) {
				grid.emplace_back(xId, blockYId, v);
			}

		}
		else { // 垂直边：X固定，遍历Y方向网格
			int ymin = v->y1, ymax = v->y2;

			if (v->x1 < box._xmin || v->x1 > box._xmax) return; // 包围盒区域外的边
			int blockXId = getBlockXId(v->x1);
			int startBlockYId = getBlockYId(ymin);
			int endBlockYId = getBlockYId(ymax);
			for (int yId = startBlockYId; yId <= endBlockYId; ++yId) {
				grid.emplace_back(blockXId, yId, v);
			}
		}
	}

	// 获取x值所处的网格列ID
	inline int getBlockXId(double x) const
	{
		int blockX = static_cast<int>((x - box._xmin) / blockWidth);
		blockX = blockX < 0 ? 0 : (blockX > blockCount ? blockCount : blockX);
		return blockX;
	}
	// 获取y值所处的网格行ID
	inline int getBlockYId(double y) const
	{
		int blockY = static_cast<int>((y - box._ymin) / blockHeight);
		blockY = blockY < 0 ? 0 : (blockY > blockCount ? blockCount : blockY);
		return blockY ;
	}
};