#pragma once

#include "../model/MVertex.h"
#include "../model/MEdge.h"
#include "../model/MPolygon.h"
#include "../model/MPolygonSet.h"
#include "../utils/HighPerfMemoryPool.h"
#include "../utils/Bbox.h"
#include "../utils/Grid.h"
#include <unordered_set>


namespace MBSO {
	using std::vector;

	class MBSOCore {
	public:
		MPolygonSet* mps1;		//多边形集A
		MPolygonSet* mps2;		//多边形集B
		MPolygonSet* resultMps;	//结果多边形集
		OP_TYPE opt;			//操作类型

		Bbox box;				//两个多边形集整体包围盒
		Bbox blkBox;			//两个多边形集相交区域包围盒

		Grid<MEdge*> grid;		//网格存边
		double blockWidth;		//当前每个小网格宽度
		double blockHeight;		//当前每个小网格高度
		int blockCount;		    //当前划分的一行或一列的网格数量，行和列数是相等的

		// 其他公用的多边形数据信息，用于网格、嵌套、绕边
		vector<MPolygon*> isolatedBeContained;		// 存交集区域包围盒内部的多边形轮廓
		vector<MPolygon*> beCollided;				// 存与交集区域包围盒碰撞的多边形轮廓（相交的+在内部的）
		vector<MPolygon*> isolated1;				// 存多边形集A: mps1的所有轮廓
		vector<MPolygon*> isolated2;				// 存多边形集B: mps2的所有轮廓
		vector<MVertex*> inPoints;					// 存所有的入点
		vector<MVertex*> outPoints;					// 存所有的出点
		int inPointsIndex;
		int outPointsIndex;

		// 记忆存在交点的边，用于将交点插入边拓扑时缩小遍历范围
		std::unordered_set<MEdge*> hasInterEdges;

		std::vector<MVertex*> equalPoints;	//用于处理端点相交的情况，size需要初始化为 2
		int curMps;	 //绕边变量，标识当前绕边的多边形集Id: 即polygonSetId

		// 内存复用相关
		HighPerfMemoryPool<MVertex> vertexsMemoryPool;  // 点内存池
		HighPerfMemoryPool<MEdge> edgesMemoryPool;		// 边内存池
		vector<MVertex*> newVertexs;			// 记录运算过程中新生成的点，运算结束后统一回收 未被结果多边形集复用的点
		vector<MEdge*> newEdges;				// 记录运算过程中新生成的边，运算结束后统一回收 未被结果多边形集复用的边

	public:
		/* 默认构造和析构 */
		MBSOCore(): mps1(new MPolygonSet), mps2(new MPolygonSet), resultMps(new MPolygonSet), opt(UNION),
					grid(101, 101), blockWidth(0), blockHeight(0), blockCount(0),
					inPointsIndex(0), outPointsIndex(0), equalPoints(2), curMps(0),
					vertexsMemoryPool(1000), edgesMemoryPool(1000)
		{};
		~MBSOCore() {
			// 释放曾经new的MPolygonSet
			if (mps1 != nullptr) delete mps1;
			if (mps2 != nullptr) delete mps2;
			if (resultMps != nullptr) {
				// 回收结果集的边和点元素，全部回收即可
				for (auto& mpoly : resultMps->mpolygons) {
					for (auto& edge : mpoly.edges)
						edgesMemoryPool.pushReuseElement(edge);
					for (auto& vertex : mpoly.vertexs)
						vertexsMemoryPool.pushReuseElement(vertex);
				}
				delete resultMps;
			}
		};
		// 禁止拷贝赋值
		MBSOCore(const MBSOCore&) = delete;
		MBSOCore& operator=(const MBSOCore&) = delete;

		/* --------------- 支持序列式运算的外部接口 --------------- */
		/* 通过 poly 设置初始多边形集 : 构造内部多边形集对象并将其赋值给 resultMps */
		void setResultMPS(const std::vector<MPoint_2>& poly);
		/* 通过 polyset 设置初始多边形集 : 构造内部多边形集对象并将其赋值给 resultMps */
		void setResultMPS(const std::vector<std::vector<MPoint_2>>& polyset);
		/* 求交集 : 与初始多边形集 resultMps 求交，结果还放在 resultMps */
		void intersect(const std::vector<MPoint_2> &poly);
		void intersect(const std::vector<std::vector<MPoint_2>> &polyset);
		/* 求并集 : 与初始多边形集 resultMps 求并，结果还放在 resultMps  */
		void join(const std::vector<MPoint_2> &poly);
		void join(const std::vector<std::vector<MPoint_2>> &polyset);
		/* 求差集 : 初始多边形集 resultMps 差 poly(set)，结果还放在 resultMps  */
		void difference(const std::vector<MPoint_2> &poly);
		void difference(const std::vector<std::vector<MPoint_2>> &polyset);
		/* 提取 resultMps 结果 */
		std::vector<std::vector<MPoint_2>> getResult();

		/* --------------- 支持二元运算外部接口 --------------- */
		std::vector<std::vector<MPoint_2>> solve(const std::vector<MPoint_2> &poly1, OP_TYPE opt, const std::vector<MPoint_2> &poly2);
		std::vector<std::vector<MPoint_2>> solve(const std::vector<std::vector<MPoint_2>> &polyset1, OP_TYPE opt, const std::vector<MPoint_2> &poly2);
		std::vector<std::vector<MPoint_2>> solve(const std::vector<MPoint_2> &poly1, OP_TYPE opt, const std::vector<std::vector<MPoint_2>> &polyset2);
		std::vector<std::vector<MPoint_2>> solve(const std::vector<std::vector<MPoint_2>> &polyset1, OP_TYPE opt, const std::vector<std::vector<MPoint_2>> &polyset2);


	private:
		/* 在指针 mps 指向的内存区域, 将多边形点集 poly 转换成 MPolygonSet* 类型，调用前确保 mps 非空且 mps->mpolygons.size() 为 0 */
		void convertToMPS(const std::vector<MPoint_2> & poly, MPolygonSet* & mps);
		/* 在指针 mps 指向的内存区域，将多边形集合的点集 polyset 转换成 MPolygonSet* 类型，调用前确保 mps 非空且 mps->mpolygons.size() 为 0*/
		void convertToMPS(const std::vector<std::vector<MPoint_2>> & polyset, MPolygonSet* & mps);
		
		/* @brief 传入两个多边形集，根据布尔运算类型OP_TYPE, 返回结果多边形集
		 * @note 调用之前，resultMps处于初始状态。求解过程中会复用mps1和mps2的可复用几何元素,将所有权交给resultMps。
		 * 求解完成后通过 memoryRecycle() 回收mps1和mps2中未被复用的几何元素，并将mps1和mps2置为初始状态
		 * “初始状态”：指该对象通过默认构造函数生成的状态（或clear()）
		 * @param mps1: 多边形集A
		 * @param mps2: 多边形集B
		 * @param resultMps: 结果多边形集
		 * @param opt: 操作类型
		 * @return void
		*/
		void solve(MPolygonSet *mps1, MPolygonSet *mps2, MPolygonSet* resultMps, OP_TYPE opt);

		//判断是否需要求解,
		bool isNeedSolve();
		//初始化工作
		void initial();
		//基于打网格求交点
		inline void caculateIntersBasedOnBlocks();
		//n方遍历暴力求交点
		inline void caculateIntersBruteForce();
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

	inline int MBSOCore::getBlockXId(double x)
	{
		int blockX = (x - blkBox.minX) / blockWidth;
		return blockX;
	}

	inline int MBSOCore::getBlockYId(double y)
	{
		int blockY = (y - blkBox.minY) / blockHeight;
		return blockY;
	}

	inline void MBSOCore::pushBackInterPoint(MVertex* p)
	{
		if (!p->isInter) return;
		if (p->interType == IN_POINT) inPoints.emplace_back(p);
		else if (p->interType == OUT_POINT) outPoints.emplace_back(p);
	}

	inline double MBSOCore::getBlockHeight()
	{
		return blockHeight;
	}

	inline MPolygonSet* MBSOCore::getResultMps()
	{
		return this->resultMps;
	}

	inline void MBSOCore::getResultMps(MPolygonSet& p)
	{
		this->resultMps->copy(p);
	}

	inline MPolygonSet* MBSOCore::getMps1()
	{
		return mps1;
	}

	inline MPolygonSet* MBSOCore::getMps2()
	{
		return mps2;
	}

	inline OP_TYPE MBSOCore::getOpt()
	{
		return OP_TYPE(this->opt);
	}

	inline int MBSOCore::getMaxX()
	{
		return box.maxX;
	}

	inline int MBSOCore::getInputSize()
	{
		return mps1->edgeCnt + mps2->edgeCnt;
	}
	inline int MBSOCore::getResultSize()
	{
		return resultMps->edgeCnt;
	}

	inline int MBSOCore::getBlockCnt()
	{
		return blockCount;
	}

	inline double MBSOCore::getBlockWidth()
	{
		return blockWidth;
	}

	inline int MBSOCore::getMaxY()
	{
		return box.getMaxY();
	}

	inline int MBSOCore::getWidth()
	{
		return box.getWidth();
	}

	inline int MBSOCore::getHeight()
	{
		return box.getHeight();
	}

	inline Bbox MBSOCore::getBox()
	{
		return box;
	}

	inline std::pair<int, int> MBSOCore::getBlockId(int x, int y)
	{
		int blockX = (x - box.minX) / blockWidth;
		int blockY = (y - box.minY) / blockHeight;
		return std::move(std::make_pair(blockX, blockY));
	}

	inline std::pair<int, int> MBSOCore::getBlockId(MPoint_2& p)
	{
		return getBlockId(p.getX(), p.getY());
	}

	inline MEdge* MBSOCore::findNextEdge(MVertex* curPt) {
		if (curMps == 0) return curPt->nextEdgeA;
		else return curPt->nextEdgeB;
	}

	inline MEdge* MBSOCore::findFrontEdge(MVertex* curPt) {
		//[csg] fix 原项目bug吧
		//if (curMps == 0) return curPt->frontEdgeB;
		if (curMps == 0) return curPt->frontEdgeA;
		else return curPt->frontEdgeB;
	}

	inline MVertex* MBSOCore::findFirstInter(INTER_TYPE interType)
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

	inline void MBSOCore::pushBackToResultMps(MPolygon& mpolygon)
	{
		if (mpolygon.edges.size() <= 2) { // 不足以形成多边形
			for (auto& edge : mpolygon.edges) {
				edge->isResultRecycle = false;
				edge->ori->isResultRecycle = false;
			}
			return;
		}
		resultMps->edgeCnt += mpolygon.edges.size();
		resultMps->box.update(mpolygon.box);
		resultMps->mpolygons.emplace_back(std::move(mpolygon));
		// 多边形标记为被结果复用
		mpolygon.isResultRecycle = true;
	}

	inline void MBSOCore::pushBackToResultMps(vector<MEdge*>& outer)
	{
		MPolygon newMPolygon(CW);
		int outerSize = outer.size();
		newMPolygon.edges.reserve(outerSize);
		newMPolygon.vertexs.reserve(outerSize);
		auto& vertexs = newMPolygon.vertexs;
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
			// 复用边
			outer[i]->setOriDest(start, end);
			// MEdge* edgePtr = edgesMemoryPool.newElement();
			// edgePtr->setOriDest(start, end);
			newMPolygon.box.update(start->point);
			// newMPolygon.edges.emplace_back(edgePtr);
			newMPolygon.edges.emplace_back(outer[i]);
			// 标记边为被结果复用
			outer[i]->isResultRecycle = true;
			// 标记点为被结果复用
			start->isResultRecycle = true;
			end->isResultRecycle = true;
		}
		pushBackToResultMps(newMPolygon);
	}
} // namespace MBSO

