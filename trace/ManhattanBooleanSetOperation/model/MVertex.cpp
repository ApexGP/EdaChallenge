#include "MVertex.h"

namespace MBSO {
	MVertex::MVertex() :
		polygonSetId(-1), polygonPtr(nullptr), isInter(false), isVisted(false),
		pointType(POINT_TYPE::POINT_2),
		nextEdgeA(nullptr), nextEdgeB(nullptr), frontEdgeA(nullptr), frontEdgeB(nullptr),
		anotherVertex(nullptr),
		isFinished(false), interType(INTER_TYPE::UNKNOWN), isResultRecycle(false)
	{
	};
	MVertex::MVertex(int _x, int _y, int _polygonSetId) :point(_x, _y), polygonSetId(_polygonSetId),
		polygonPtr(nullptr), isInter(false), isVisted(false), pointType(POINT_TYPE::POINT_2),
		nextEdgeA(nullptr), nextEdgeB(nullptr), frontEdgeA(nullptr), frontEdgeB(nullptr),
		anotherVertex(nullptr),
		isFinished(false), interType(INTER_TYPE::UNKNOWN), isResultRecycle(false)
	{
	};
	MVertex::MVertex(const MPoint_2& _p) :point(_p), polygonSetId(-1), polygonPtr(nullptr),
		isInter(false), isVisted(false), pointType(POINT_TYPE::POINT_2),
		nextEdgeA(nullptr), nextEdgeB(nullptr), frontEdgeA(nullptr), frontEdgeB(nullptr),
		anotherVertex(nullptr),
		isFinished(false), interType(INTER_TYPE::UNKNOWN), isResultRecycle(false)
	{
	};

	MVertex::~MVertex() {}

	// 完全重置顶点状态
	void MVertex::resetAll()
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
		isResultRecycle = false;
	}
	// 重置部分内部标记，保留拓扑信息 （可用于对象重用时，其拓扑信息在外部明确后直接赋值，内部标记通过resetFlags重置）
	void MVertex::resetFlags()
	{
		isInter = false;
		pointType = POINT_2;
		anotherVertex = nullptr;
		interP = MPoint_2();
		interType = UNKNOWN;
		isVisted = isFinished = false;
		isResultRecycle = false;
	}
}