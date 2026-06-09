#pragma once
#include "MEdge.h"
#include "MVertex.h"
#include "MPolygon.h"
#include "../utils/Bbox.h"


namespace MBSO {
	// 曼哈顿多边形集类
	class MPolygonSet {
	public:
		std::vector<MPolygon> mpolygons;// 存储集合所有的多边形轮廓
		Bbox box;						// 整个集合的包围盒
		int edgeCnt;					// 该多边形集包含的边数
		bool isNeedResetStatus;			// 是否需要重置该多边形集内部的几何元素状态
										/*(利用标志位惰性初始化，比如产生结果集后，将标志位置true，
										   如果它被用于参与下次运算，就会真正的被调用初始化，如果它不参与后续运算了，那么内部状态无须重置，节省开销) */

		MPolygonSet() { edgeCnt = 0; isNeedResetStatus = true; }
		~MPolygonSet() {}

		// 初始化多边形集内部几何元素状态
		void resetStatus() {
			if (!isNeedResetStatus) return;
			edgeCnt = 0;
			box.reset();
			// 初始化每个多边形
			for (int i = 0; i < mpolygons.size(); ++i)
			{
				auto& mpoly = mpolygons[i];
				mpoly.dir = CW;
				mpoly.resetStatus(); // 重置各个多边形内部几何元素状态
				box.update(mpoly.box);
				edgeCnt += mpoly.edges.size();
			}
			isNeedResetStatus = false;
		}

		// 设置多边形集内部几何元素的polygonSetId
		void setPolygonSetId(int _polygonSetId) {
			for (int i = 0; i < mpolygons.size(); ++i)
			{
				auto& mpoly = mpolygons[i];
				mpoly.polygonSetId = _polygonSetId;

				auto& edges = mpoly.edges;
				int n = edges.size();
				for (int i = 0; i < n; ++i) {
					// 点的
					auto vertex = edges[i]->ori; // 取边起点
					vertex->polygonSetId = _polygonSetId;
					// 边的
					edges[i]->polygonSetId = _polygonSetId;
				}
			}
		}

		// 获取多边形集中的多边形数量
		int getMPolygonSize() const
		{
			return mpolygons.size();
		}
		void clear() {
			mpolygons.clear();
			box.reset();
			edgeCnt = 0;
		}

		// 将当前多边形集的内容移动到另一个多边形集p中，当前多边形集将被清空
		void copy(MPolygonSet& p)
		{
			p.mpolygons = std::move(mpolygons);
			p.box = box;
			p.edgeCnt = edgeCnt;
		}

	};

} // namespace MBSP