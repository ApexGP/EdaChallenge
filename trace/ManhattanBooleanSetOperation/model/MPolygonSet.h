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
		// 使用多边形集的点集构造MPolygonSet对象，请确保输入参数 _mps 中每个多边形的点是顺时针顺序 
		MPolygonSet(const std::vector<std::vector<MPoint_2>>& _mps) { 
			edgeCnt = 0; 
			// 依次构造每个多边形
			mpolygons.reserve(_mps.size());
			for (const auto& _mpoly : _mps) {
				int _mpolySize = _mpoly.size(); // 该多边形边数
				// 创建多边形
				MPolygon mpolygon;
				mpolygon.edges.reserve(_mpolySize);
				// 构造点集
				std::vector<MVertex*> vertexs;
				vertexs.reserve(_mpolySize);
				for (const auto& _mpoint : _mpoly) {
					MVertex* mvertex = new MVertex(_mpoint);
					vertexs.emplace_back(mvertex);
				}
				// 基于点集构造边集
				for (int i = 0; i < _mpolySize; ++i) {
					const auto& start = vertexs[i];
					const auto& end = vertexs[(i + 1) % _mpolySize];
					MEdge* medge = new MEdge(start, end);
					mpolygon.edges.emplace_back(medge);
					mpolygon.box.update(start->point);
				}
				// 建立点到边的连接
				for (int i = 0; i < _mpolySize; ++i)
				{
					const auto& start = vertexs[i];
					const auto& end = vertexs[(i + 1) % _mpolySize];
					start->nextEdgeA = start->nextEdgeB = mpolygon.edges[i];
					end->frontEdgeA = end->frontEdgeB = mpolygon.edges[i];
				}
				// 放入多边形集中
				box.update(mpolygon.box);
				mpolygons.emplace_back(std::move(mpolygon));
				edgeCnt += _mpolySize;
			}
		}
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