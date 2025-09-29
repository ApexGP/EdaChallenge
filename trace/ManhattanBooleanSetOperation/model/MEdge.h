#pragma once
#include "MVertex.h"
#include "MPolygon.h"
#include "../utils/MSegment_2.h"
#include <vector>

namespace MBSO {
	class MVertex;
	class MPolygon;

	// 曼哈顿边拓扑信息类
	class MEdge {
	public:
		MVertex* ori;				// 起点指针
		MVertex* dest;				// 终点指针
		int polygonSetId;			// 所属曼哈顿多边形集的id （0或1，多边形集A则为0，多边形集B则为1）
		MPolygon* polygonPtr;		// 所属曼哈顿多边形的指针
		MSegment_2 seg;				// 二维曼哈顿线段
		std::vector<MVertex*> interPoints;	// 交点插入排序

		MEdge();
		MEdge(MVertex* s, MVertex* e);

		// 重置部分内部标记，保留拓扑信息 （可用于对象重用时，其拓扑信息在外部明确后直接赋值，部分内部标记通过init重置）
		void init();
		// 反转边的方向
		void reverse();
		// 设置边的起点和终点
		void setOriDest(MVertex* _ori, MVertex* _dest, int _polygonSetId = 0);
		// 插入交点
		void push_interPoint(MVertex* p);
	};

	
	MEdge::MEdge() : ori(nullptr), dest(nullptr), polygonSetId(-1), polygonPtr(nullptr)
	{}

	MEdge::MEdge(MVertex* s, MVertex* e) : ori(s), dest(e), polygonSetId(-1), polygonPtr(nullptr), seg(s->point, e->point)
	{}

	void MEdge::init()
	{
		seg.setSeg(ori->point, dest->point);
		interPoints.clear();
	}

	void MEdge::reverse()
	{
		std::swap(ori, dest);
		seg.reverse();
	}

	void MEdge::setOriDest(MVertex* _ori, MVertex* _dest, int _polygonSetId)
	{
		ori = _ori;
		dest = _dest;
		polygonSetId = _polygonSetId;
		seg.setSeg(ori->point, dest->point);
	}

	void MEdge::push_interPoint(MVertex* p)
	{
		interPoints.emplace_back(p);
	}
} // namespace MBSO
