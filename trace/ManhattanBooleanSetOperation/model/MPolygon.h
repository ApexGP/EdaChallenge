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

} // namespace MBSO
