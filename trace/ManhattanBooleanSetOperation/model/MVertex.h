#pragma once
#include "../utils/MPoint_2.h"

namespace MBSO {
	class MPolygon;
	class MEdge;

	// 曼哈顿顶点拓扑信息类
	class MVertex {
	public:
		MPoint_2 point;					// 二维平面点
		int polygonSetId;				// 所属曼哈顿多边形集的id（0或1，多边形集A则为0，多边形集B则为1）
		MPolygon* polygonPtr;			// 所属曼哈顿多边形的指针
		bool isInter;					// 是否是交点
		POINT_TYPE pointType;			// 顶点类型
		MEdge* nextEdgeA, * frontEdgeA;	// 多边形A上的前后两条边
		MEdge* nextEdgeB, * frontEdgeB;	// 多边形B上的前后两条边
		MVertex* anotherVertex;			// 二重交点，另一个顶点
		MPoint_2 interP;				// 临时存储真实交点
		INTER_TYPE interType;			// 出入点类型
		bool isVisted;					// 是否被访问过
		bool isFinished;				// 拆点是否被拆过

		MVertex() :
			polygonSetId(-1), polygonPtr(nullptr), isInter(false), isVisted(false),
			pointType(POINT_TYPE::POINT_2),
			nextEdgeA(nullptr), nextEdgeB(nullptr), frontEdgeA(nullptr), frontEdgeB(nullptr)
			, anotherVertex(nullptr)
			, isFinished(false), interType(INTER_TYPE::UNKNOWN)
		{};
		MVertex(int _x, int _y, int _polygonSetId) :point(_x, _y), polygonSetId(_polygonSetId),
			polygonPtr(nullptr), isInter(false), isVisted(false), pointType(POINT_TYPE::POINT_2),
			nextEdgeA(nullptr), nextEdgeB(nullptr), frontEdgeA(nullptr), frontEdgeB(nullptr)
			, anotherVertex(nullptr)
			, isFinished(false), interType(INTER_TYPE::UNKNOWN)
		{};
		MVertex(const MPoint_2& _p) :point(_p), polygonSetId(-1), polygonPtr(nullptr),
			isInter(false), isVisted(false), pointType(POINT_TYPE::POINT_2),
			nextEdgeA(nullptr), nextEdgeB(nullptr), frontEdgeA(nullptr), frontEdgeB(nullptr)
			, anotherVertex(nullptr)
			, isFinished(false), interType(INTER_TYPE::UNKNOWN)
		{};

		~MVertex() {}

		// 重写等号
		bool operator==(const MVertex& vertex) const { return vertex.point == this->point; }
		bool operator<(const MVertex& vertex) const { return this->point < vertex.point; }
		bool operator>(const MVertex& vertex) const { return vertex < (*this); }
		bool operator>=(const MVertex& vertex) const { return *this > vertex || *this == vertex; }

		// 完全重置顶点状态
		void reset()
		{
			point = MPoint_2();
			polygonSetId = -1;
			polygonPtr = nullptr;
			isInter = false;
			pointType = POINT_2;
			nextEdgeA = frontEdgeA = nullptr;
			nextEdgeB = frontEdgeB = nullptr;
			anotherVertex = nullptr;
			interP = MPoint_2();
			interType = UNKNOWN;
			isVisted = isFinished = false;
		}
		// 重置部分内部标记，保留拓扑信息 （可用于对象重用时，其拓扑信息在外部明确后直接赋值，部分内部标记通过init重置）
		void init()
		{
			isInter = false;
			pointType = POINT_2;
			anotherVertex = nullptr;
			interType = UNKNOWN;
			isVisted = isFinished = false;
		}
	};

} // namespace MBSO