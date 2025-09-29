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


		MPolygonSet() { edgeCnt = 0; }
		MPolygonSet(const std::vector<std::vector<MPoint_2>>& a) { edgeCnt = 0; }
		~MPolygonSet() {}

		// 初始化多边形集
		void init(int polygonSetId) {
			edgeCnt = 0;
			box.reset();
			// 初始化每个多边形
			for (int i = 0; i < mpolygons.size(); ++i)
			{
				auto& outer = mpolygons[i];
				outer.polygonSetId = polygonSetId;
				outer.id = i;
				outer.dir = CW;
				outer.existInterPoint = false;
				outer.isNested = false;
				outer.init();
				box.update(outer.box);
				edgeCnt += outer.edges.size();
			}
		}

		// 获取多边形集中的多边形数量
		int getMPolygonSize()
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