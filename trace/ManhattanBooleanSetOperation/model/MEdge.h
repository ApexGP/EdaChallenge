#pragma once
#include "MVertex.h"
#include "../utils/MSegment_2.h"
#include <vector>

namespace MBSO {
	struct MVertex;
	struct MPolygon;

	// 曼哈顿边拓扑信息结构体
	struct MEdge {
		/* 拓扑信息 */
		MVertex* ori;				// 起点指针
		MVertex* dest;				// 终点指针
		int polygonSetId;			// 所属曼哈顿多边形集的id （0或1，多边形集A则为0，多边形集B则为1）
		MPolygon* polygonPtr;		// 所属曼哈顿多边形的指针
		/* 标记信息 */
		MSegment_2 seg;				// 二维曼哈顿线段
		std::vector<MVertex*> interPoints;	// 交点插入排序
		bool isResultRecycle;       // 是否被结果多边形集复用（用于判断是否需要内存回收）

		MEdge();
		MEdge(MVertex* s, MVertex* e);
		MEdge(MVertex* s, MVertex* e, int _polygonSetId, MPolygon* polygonPtr);

		// 完全重置边状态
		void resetAll();
		// 重置部分内部标记，保留拓扑信息 （可用于对象重用时，其拓扑信息在多边形集中明确后直接赋值，部分内部标记通过resetFlags重置）
		void resetFlags();
		// 反转边的方向
		void reverse();
		// 设置边的起点和终点
		void setOriDest(MVertex* _ori, MVertex* _dest);
		void setOriDest(MVertex* _ori, MVertex* _dest, int _polygonSetId);
		// 插入交点
		void push_interPoint(MVertex* p);
	};
} // namespace MBSO
