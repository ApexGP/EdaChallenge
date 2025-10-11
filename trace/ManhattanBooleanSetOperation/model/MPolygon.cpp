#include "MPolygon.h"

namespace MBSO {

	MPolygon::~MPolygon() {}
	MPolygon::MPolygon(DIR _dir) : dir(_dir),
		polygonSetId(0), existInterPoint(false),
		avgLength(0), isNested(false), isResultRecycle(false)
	{
	}

	MPolygon& MPolygon::operator=(const MPolygon& mpolygon)
	{
		if (this == &mpolygon) return *this;
		vertexs = mpolygon.vertexs;
		edges = mpolygon.edges;
		dir = mpolygon.dir;
		polygonSetId = mpolygon.polygonSetId;
		existInterPoint = mpolygon.existInterPoint;
		isNested = mpolygon.isNested;
		box = mpolygon.box;
		avgLength = mpolygon.avgLength;
		isResultRecycle = mpolygon.isResultRecycle;
		return *this;
	}
	MPolygon::MPolygon(const MPolygon& mpolygon)
	{
		if (this == &mpolygon) return;
		vertexs = mpolygon.vertexs;
		edges = mpolygon.edges;
		dir = mpolygon.dir;
		polygonSetId = mpolygon.polygonSetId;
		existInterPoint = mpolygon.existInterPoint;
		isNested = mpolygon.isNested;
		box = mpolygon.box;
		avgLength = mpolygon.avgLength;
		isResultRecycle = mpolygon.isResultRecycle;
	}
	MPolygon& MPolygon::operator=(MPolygon&& mpolygon) noexcept {
		if (this != &mpolygon) {
			vertexs = move(mpolygon.vertexs);
			edges = move(mpolygon.edges);
			dir = mpolygon.dir;
			polygonSetId = mpolygon.polygonSetId;
			existInterPoint = mpolygon.existInterPoint;
			isNested = mpolygon.isNested;
			box = mpolygon.box;
			avgLength = mpolygon.avgLength;
			isResultRecycle = mpolygon.isResultRecycle;
		}
		return *this;
	}
	MPolygon::MPolygon(MPolygon&& mpolygon) noexcept
	{
		if (this == &mpolygon) return;
		vertexs = move(mpolygon.vertexs);
		edges = move(mpolygon.edges);
		dir = mpolygon.dir;
		polygonSetId = mpolygon.polygonSetId;
		existInterPoint = mpolygon.existInterPoint;
		isNested = mpolygon.isNested;
		box = mpolygon.box;
		avgLength = mpolygon.avgLength;
		isResultRecycle = mpolygon.isResultRecycle;
	}

	void MPolygon::resetStatus()
	{
		existInterPoint = false;
		isNested = false;
		box.reset();
		avgLength = 0;
		isResultRecycle = false;

		int n = edges.size();
		if (n == 0)return;
		// 遍历边集
		for (int i = 0; i < n; ++i) {
			// 点相关初始化
			auto vertex = edges[i]->ori; // 取边起点
			edges[i]->dest = edges[(i + 1) % n]->ori; // 连接
			vertex->resetFlags();
			vertex->polygonPtr = this;
			vertex->nextEdgeA = vertex->nextEdgeB = edges[i];
			vertex->frontEdgeA = vertex->frontEdgeB = edges[(i - 1 + n) % n];
			// 边相关初始化
			edges[i]->resetFlags();
			edges[i]->polygonPtr = this;
			// 轮廓相关初始化
			avgLength += edges[i]->seg.getLength();
			box.update(vertex->point);
		}
		avgLength /= n;
	}

	bool MPolygon::isInside(const MPoint_2& point)
	{
		// 射线法判断点是否在曼哈顿多边形内部（不包含边界）
		int crossings = 0;
		int x = point.getX();
		int y = point.getY();
		// 检测水平射线（向右）与边的交点数
		for (size_t i = 0; i < edges.size(); ++i) {
			int x1 = edges[i]->ori->point.getX();
			int y1 = edges[i]->ori->point.getY();
			int x2 = edges[i]->dest->point.getX();
			int y2 = edges[i]->dest->point.getY();

			// 对于曼哈顿多边形，边要么是水平的要么是垂直的
			if (y1 == y2) continue; // 水平边，跳过
			if (x1 < x) continue;   // 射线左边的垂直边，跳过
			if ((y1 <= y && y < y2) || (y2 <= y && y < y1)) { // 如果射线与其右边的垂直边y范围有交集
				crossings++;
			}
		}
		return (crossings % 2) == 1;
	}

} // namespace MBSO
