#pragma once
#include <vector>
#include "../utils/Bbox.h"
#include "MVertex.h"
#include "MEdge.h"

namespace MBSO {
	struct MVertex;
	struct MEdge;

	// 曼哈顿多边形轮廓结构体
	struct MPolygon
	{
		/* 拓扑信息 */
		std::vector<MVertex*> vertexs;// 轮廓所包含的所有的点（其实边数组已经够用，多存点数组为了方便内存回收）
		std::vector<MEdge*> edges;	// 轮廓所包含的所有的边
		int polygonSetId;			// 所属曼哈顿多边形集的id （0或1，多边形集A则为0，多边形集B则为1）
		DIR dir;					// 类型(外轮廓or空洞)
		/* 标记信息 */
		bool existInterPoint;		// 标记是否是相交轮廓，没被访问过则是孤立轮廓，需要确定包含关系
		bool isNested;				// 是否已经确定嵌套关系了
		Bbox box;					// 包围盒
		double avgLength;			// 边的平均长度
		bool isResultRecycle;       // 是否被结果多边形集复用（用于判断是否需要内存回收）


		MPolygon(DIR _dir = DIR::CW);
		~MPolygon();

		MPolygon& operator=(const MPolygon& mpolygon);
		MPolygon(const MPolygon& mpolygon);
		MPolygon& operator=(MPolygon&& mpolygon) noexcept;
		MPolygon(MPolygon&& mpolygon) noexcept;

		// 重置部或初始化内部几何元素状态
		void resetStatus();

		// 判断两个轮廓的包含关系的时候，如果不存在交点，那么就需要判断某个点是否在另一个多边形内部
		// 射线法判断点是否在曼哈顿多边形内部（即不包含边界）
		bool isInside(const MPoint_2& point);
	};

} // namespace MBSO
