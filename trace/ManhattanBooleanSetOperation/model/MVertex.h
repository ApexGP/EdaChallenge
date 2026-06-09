#pragma once
#include "../utils/MPoint_2.h"

namespace MBSO {
	struct MPolygon;
	struct MEdge;

	// 曼哈顿顶点拓扑信息结构体
	struct MVertex {
		/* 拓扑信息 */
		MPoint_2 point;					// 二维平面点
		int polygonSetId;				// 所属曼哈顿多边形集的id（0或1，多边形集A则为0，多边形集B则为1）
		MPolygon* polygonPtr;			// 所属曼哈顿多边形的指针
		MEdge* nextEdgeA, * frontEdgeA;	// 多边形A上的前后两条边
		MEdge* nextEdgeB, * frontEdgeB;	// 多边形B上的前后两条边
		/* 标记信息 */
		bool isInter;					// 是否是交点
		POINT_TYPE pointType;			// 顶点类型
		MVertex* anotherVertex;			// 二重交点，另一个顶点
		MPoint_2 interP;				// 临时存储真实交点
		INTER_TYPE interType;			// 出入点类型
		bool isVisted;					// 是否被访问过
		bool isFinished;				// 拆点是否被拆过
		bool isResultRecycle;           // 是否被结果多边形集复用（用于判断是否需要内存回收）

		MVertex();
		MVertex(int _x, int _y, int _polygonSetId);
		MVertex(const MPoint_2& _p);
		MVertex(const MVertex&) = default;
		~MVertex();

		// 重写等号
		bool operator==(const MVertex& vertex) const { return vertex.point == this->point; }
		bool operator<(const MVertex& vertex) const { return this->point < vertex.point; }
		bool operator>(const MVertex& vertex) const { return vertex < (*this); }
		bool operator>=(const MVertex& vertex) const { return *this > vertex || *this == vertex; }

		// 完全重置顶点状态
		void resetAll();
		// 重置部分内部标记，保留拓扑信息 （可用于对象重用时，其拓扑信息在多边形集中明确后直接赋值，内部标记通过resetFlags重置）
		void resetFlags();
	};

} // namespace MBSO