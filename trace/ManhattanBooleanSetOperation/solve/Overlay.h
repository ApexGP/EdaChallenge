#pragma once

#include "../model/MVertex.h"
#include "../model/MEdge.h"
#include "../model/MPolygon.h"
#include "../model/MPolygonSet.h"
#include "../utils/Bbox.h"
#include "../utils/Grid.h"
#include <unordered_set>


namespace MBSO {
	using std::vector;

	class Overlay {
	public:
		MPolygonSet* mps1;		//多边形集A
		MPolygonSet* mps2;		//多边形集B
	private:
		MPolygonSet* resultMps;	//结果多边形集
		OP_TYPE opt;			//操作类型

		Bbox box;				//两个多边形集整体包围盒
		Bbox blkBox;			//两个多边形集相交区域包围盒
		Grid<MEdge*> grid;		//网格
		double blockWidth, blockHeight; // 网格大小
		int blockCount;		    // 网格数量，一行或一列的网格数量，行和列数相等

		int curMps;				//走边公共变量 标识当前走边多边形集Id: 即polygonSetId

		std::vector<MVertex*> equalPoints;	// 需要初始化为 2

		// 记忆存在交点的边，缩小遍历范围
		std::unordered_set<MEdge*> hasInterEdges;

		// 内存回收相关，需要把临时 new 出来的放进去，把非孤立轮廓的边，全放入空闲链表。
		vector<MEdge*> noNeedSegs;

		// 公用的多边形数据信息
		vector<MPolygon*> isolatedBeContained;		// 存交集区域包围盒内部的多边形轮廓
		vector<MPolygon*> beCollided;				// 存与交集区域碰撞的多边形轮廓
		vector<MPolygon*> isolated1;				// 存多边形集A: mps1的所有轮廓
		vector<MPolygon*> isolated2;				// 存多边形集B: mps2的所有轮廓
		vector<MVertex*> inPoints;					// 存所有的入点
		vector<MVertex*> outPoints;					// 存所有的出点
		int inPointsIndex;
		int outPointsIndex;

	public:
		Overlay() : grid(101, 101), equalPoints(2) {};
		~Overlay();

		/* @brief 传入两个多边形集，根据布尔运算类型OP_TYPE,返回结果多边形集
		 * @param mps1: 多边形集A
		 * @param mps2: 多边形集B
		 * @param resultMps: 结果多边形集
		 * @param opt: 操作类型
		 * @return void
		*/
		void solve(MPolygonSet*, MPolygonSet*, MPolygonSet*, OP_TYPE);

		//判断是否需要求解
		bool isNeedSolve();
		//初始化工作
		void initial();
		//基于打网格求交点
		inline void caculateIntersBasedOnBlocks();
		//交点插入轮廓，并确定所有交点的LR指针
		void insertAnddealLR();  
		//处理嵌套
		void dealIsolatedMPolygon();
		//走边
		void crossEdges();
		// 内存回收
		void memoryRecycle();

		//根据相交多边形平均边长和包围盒交集区域X轴跨度自适应计算网格大小
		void chooseBlkCntBasedOnInterRegion();
		//找出该边经过的所有交集区域内的网格
		inline void findEdgePassedBlockOnInterRegion(MEdge* v);
		//自己实现的曼哈顿线段求交
		bool caculateInterSSself(MEdge*, MEdge*); 
		//处理线段相交得到的交点
		void dealSSInterWhenPoint(MEdge*, MEdge*, MPoint_2& p);
		//交点插入出入点数组
		void pushBackInterPoint(MVertex* p);

		//针对两个端点相交 和 一个端点和一个线段相交 这两种情况，进行拆点（将一个点拆成两个点）
		void splitPoint(MVertex* point);   

		//走边相关
		MVertex* findFirstInter(INTER_TYPE interType = OUT_POINT);
		MEdge* findNextEdge(MVertex* curPt);
		MEdge* findFrontEdge(MVertex* curPt);

		//截取一部分轮廓
		void interceptOuter(vector<MEdge*>&, MVertex* start);	

		//将轮廓加入结果多边形集
		void pushBackToResultMps(MPolygon& mpolygon);
		void pushBackToResultMps(vector<MEdge*>& outer);
		

		/* getter and setter*/
		// 获取输入和结果多边形集的边数
		int getInputSize();
		int getResultSize();
		// 获取网格相关信息
		int getBlockCnt();
		double getBlockWidth();
		double getBlockHeight();
		// 获取结果多边形集
		MPolygonSet* getResultMps();
		void getResultMps(MPolygonSet& p);
		// 获取输入多边形集
		MPolygonSet* getMps1();
		MPolygonSet* getMps2();
		// 获取操作类型
		OP_TYPE getOpt();
		// 获取多边形集包围盒相关信息
		int getMaxX();
		int getMaxY();
		int getWidth();
		int getHeight();
		Bbox getBox();
		// 获取点所处的网格ID
		std::pair<int, int> getBlockId(int, int);
		std::pair<int, int> getBlockId(MPoint_2& p);
		// 获取x值所处的网格列ID
		int getBlockXId(double x);
		// 获取y值所处的网格行ID
		int getBlockYId(double y);
	};

	inline int Overlay::getBlockXId(double x)
	{
		int blockX = (x - blkBox.minX) / blockWidth;
		return blockX;
	}

	inline int Overlay::getBlockYId(double y)
	{
		int blockY = (y - blkBox.minY) / blockHeight;
		return blockY;
	}

	inline void Overlay::pushBackInterPoint(MVertex* p)
	{
		if (!p->isInter) return;
		if (p->interType == IN_POINT) inPoints.emplace_back(p);
		else if (p->interType == OUT_POINT) outPoints.emplace_back(p);
	}

	inline double Overlay::getBlockHeight()
	{
		return blockHeight;
	}

	inline MPolygonSet* Overlay::getResultMps()
	{
		return this->resultMps;
	}

	inline void Overlay::getResultMps(MPolygonSet& p)
	{
		this->resultMps->copy(p);
	}

	inline MPolygonSet* Overlay::getMps1()
	{
		return mps1;
	}

	inline MPolygonSet* Overlay::getMps2()
	{
		return mps2;
	}

	inline OP_TYPE Overlay::getOpt()
	{
		return OP_TYPE(this->opt);
	}

	inline int Overlay::getMaxX()
	{
		return box.maxX;
	}

	inline int Overlay::getInputSize()
	{
		return mps1->edgeCnt + mps2->edgeCnt;
	}
	inline int Overlay::getResultSize()
	{
		return resultMps->edgeCnt;
	}

	inline int Overlay::getBlockCnt()
	{
		return blockCount;
	}

	inline double Overlay::getBlockWidth()
	{
		return blockWidth;
	}

	inline int Overlay::getMaxY()
	{
		return box.getMaxY();
	}

	inline int Overlay::getWidth()
	{
		return box.getWidth();
	}

	inline int Overlay::getHeight()
	{
		return box.getHeight();
	}

	inline Bbox Overlay::getBox()
	{
		return box;
	}

	inline std::pair<int, int> Overlay::getBlockId(int x, int y)
	{
		int blockX = (x - box.minX) / blockWidth;
		int blockY = (y - box.minY) / blockHeight;
		return std::move(std::make_pair(blockX, blockY));
	}

	inline std::pair<int, int> Overlay::getBlockId(MPoint_2& p)
	{
		return getBlockId(p.getX(), p.getY());
	}

	inline MEdge* Overlay::findNextEdge(MVertex* curPt) {
		if (curMps == 0) return curPt->nextEdgeA;
		else return curPt->nextEdgeB;
	}

	inline MEdge* Overlay::findFrontEdge(MVertex* curPt) {
		//[csg] fix 原项目bug吧
		//if (curMps == 0) return curPt->frontEdgeB;
		if (curMps == 0) return curPt->frontEdgeA;
		else return curPt->frontEdgeB;
	}

	inline MVertex* Overlay::findFirstInter(INTER_TYPE interType)
	{
		if (interType == OUT_POINT) {
			while (outPointsIndex < outPoints.size() && outPoints[outPointsIndex]->isVisted) {
				outPointsIndex++;
			}
			if (outPointsIndex >= outPoints.size()) return nullptr;
			return outPoints[outPointsIndex];
		}
		else if (interType == IN_POINT) {
			while (inPointsIndex < inPoints.size() && inPoints[inPointsIndex]->isVisted) {
				inPointsIndex++;
			}
			if (inPointsIndex >= inPoints.size()) return nullptr;
			return inPoints[inPointsIndex];
		}
		return nullptr;
	}

	inline void Overlay::pushBackToResultMps(MPolygon& mpolygon)
	{
		if (mpolygon.edges.size() == 0) return;
		resultMps->edgeCnt += mpolygon.edges.size();
		resultMps->box.update(mpolygon.box);
		int size = mpolygon.edges.size();
		if (mpolygon.dir == CW)
			resultMps->mpolygons.emplace_back(std::move(mpolygon));
	}

	inline void Overlay::pushBackToResultMps(vector<MEdge*>& outer)
	{
		MPolygon newMPolygon(CW);
		newMPolygon.isNeedInit = true;
		int outerSize = outer.size();
		vector<MVertex*> vertexs;
		newMPolygon.edges.reserve(outerSize);
		vertexs.reserve(outerSize);
		for (auto& edge : outer) {
			if (edge->seg.getLength() == 0) continue;
			MVertex* cur;
			if (edge->polygonSetId == 1 && opt == DIFF) cur = edge->dest;
			else cur = edge->ori;
			vertexs.emplace_back(cur);
		}
		outerSize = vertexs.size();
		for (int i = 0; i < outerSize; ++i) {
			auto start = vertexs[i];
			auto end = vertexs[(i + 1) % outerSize];
			MEdge* edgePtr = new MEdge();
			edgePtr->setOriDest(start, end);
			newMPolygon.box.update(start->point);
			edgePtr->interPoints.clear();
			newMPolygon.edges.emplace_back(edgePtr);
		}
		pushBackToResultMps(newMPolygon);
	}
} // namespace MBSO

