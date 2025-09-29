#pragma once
#include <vector>
#include "../utils/Bbox.h"
#include "MVertex.h"
#include "MEdge.h"

namespace MBSO {
	class MVertex;
	class MEdge;

	// 曼哈顿多边形轮廓类
	class MPolygon
	{
	public:
		std::vector<MEdge*> edges;	// 轮廓所包含的所有的边
		MVertex* startPt;			// 轮廓绕边起点
		DIR dir;					// 类型(外轮廓or空洞)
		int polygonSetId;			// 所属曼哈顿多边形集的id （0或1，多边形集A则为0，多边形集B则为1）
		int id;						// 在曼哈顿多边形集中的轮廓编号
		bool existInterPoint;		// 标记是否是相交轮廓，没被访问过则是孤立轮廓，需要确定包含关系
		bool isNested;				// 是否已经确定嵌套关系了
		Bbox box;					// 包围盒
		bool isNeedInit = true;		// 是否需要初始化（用于惰性初始化，仅在结构变化时标记需要重新初始化；后续重用对象时，才触发初始化；若不在重用，也省去重置的时间）
		bool isReverse;				// 是否发生了翻转
		double avgLength;			// 边的平均长度

		MPolygon(DIR _dir = DIR::CW);
		~MPolygon();

		MPolygon& operator=(const MPolygon& mpolygon);
		MPolygon(const MPolygon& mpolygon);
		MPolygon(MPolygon&& mpolygon) noexcept;

		// 翻转轮廓方向
		void reverse();
		// 重置部分内部标记，保留拓扑信息 （可用于对象重用时，其拓扑信息在外部明确后直接赋值，部分内部标记通过init重置）
		void init();

		// 判断两个轮廓的包含关系的时候，如果不存在交点，那么就需要判断某个点是否在另一个多边形内部
		// 射线法判断点是否在曼哈顿多边形内部（即不包含边界）
		bool isInside(const MPoint_2& point);
	};

	MPolygon::~MPolygon(){}
	MPolygon::MPolygon(DIR _dir) : dir(_dir), startPt(nullptr),
		id(0), polygonSetId(0), existInterPoint(false),
		avgLength(0), isNeedInit(true), isNested(false), isReverse(false)
	{}

	MPolygon& MPolygon::operator=(const MPolygon& mpolygon)
	{
		if (this == &mpolygon) return *this;
		startPt = mpolygon.startPt;
		edges = mpolygon.edges;
		dir = mpolygon.dir;
		polygonSetId = mpolygon.polygonSetId;
		id = mpolygon.id;
		existInterPoint = mpolygon.existInterPoint;
		isNested = mpolygon.isNested;
		box = mpolygon.box;
		isNeedInit = mpolygon.isNeedInit;
		isReverse = mpolygon.isReverse;
		avgLength = mpolygon.avgLength;
		return *this;
	}
	MPolygon::MPolygon(const MPolygon& mpolygon)
	{
		if (this == &mpolygon) return;
		startPt = mpolygon.startPt;
		edges = mpolygon.edges;
		dir = mpolygon.dir;
		polygonSetId = mpolygon.polygonSetId;
		id = mpolygon.id;
		existInterPoint = mpolygon.existInterPoint;
		isNested = mpolygon.isNested;
		box = mpolygon.box;
		isNeedInit = mpolygon.isNeedInit;
		isReverse = mpolygon.isReverse;
		avgLength = mpolygon.avgLength;
	}
	MPolygon::MPolygon(MPolygon&& mpolygon) noexcept
	{
		if (this == &mpolygon) return;
		startPt = mpolygon.startPt;
		mpolygon.startPt = nullptr;
		edges = move(mpolygon.edges);
		dir = mpolygon.dir;
		polygonSetId = mpolygon.polygonSetId;
		id = mpolygon.id;
		existInterPoint = mpolygon.existInterPoint;
		isNested = mpolygon.isNested;
		box = mpolygon.box;
		isNeedInit = mpolygon.isNeedInit;
		isReverse = mpolygon.isReverse;
		avgLength = mpolygon.avgLength;
	}

	// 翻转
	void MPolygon::reverse()
	{
		if (polygonSetId == 0) return; // 只能翻转 B
		std::reverse(edges.begin(), edges.end());
		isNeedInit = true;			// 翻转之后需要重新初始化一遍边。
		int n = edges.size();
		for (int i = 0; i < n; ++i) {
			edges[i]->reverse();
		}
		dir = (dir == CW ? CCW : CW);
	}


	void MPolygon::init()
	{
		if (!isNeedInit) return;	// 不需要初始化，直接return
		int n = edges.size();
		if (n == 0) return;
		startPt = edges[0]->ori;
		avgLength = 0;
		// 遍历拓扑边列表，初始化边和点拓扑结构
		for (int i = 0; i < n; ++i) {
			// 点相关初始化
			auto vertex = edges[i]->ori; // 取边起点
			edges[i]->dest = edges[(i + 1) % n]->ori;
			vertex->init();
			vertex->nextEdgeA = vertex->nextEdgeB = edges[i];
			vertex->frontEdgeA = vertex->frontEdgeB = edges[(i - 1 + n) % n];
			vertex->polygonPtr = this;
			vertex->polygonSetId = polygonSetId;

			// 边相关初始化
			edges[i]->init();
			edges[i]->polygonPtr = this;
			edges[i]->polygonSetId = polygonSetId;

			// 轮廓相关初始化
			avgLength += edges[i]->seg.getLength();
		}
		avgLength /= n;
		isNeedInit = false;
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
