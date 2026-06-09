#include "./MBSOCore.h"

namespace MBSO {
	using std::min;
	using std::max;

	/* --------------- 支持序列式运算的外部接口 --------------- */
	void MBSOCore::setResultMPS(const std::vector<MPoint_2>& poly) {
		// 回收边和点:结果集的元素，全部回收即可
		for (auto& mpoly : resultMps->mpolygons) {
			for (auto& edge : mpoly.edges)
				edgesMemoryPool.pushReuseElement(edge);
			for (auto& vertex : mpoly.vertexs)
				vertexsMemoryPool.pushReuseElement(vertex);
		}
		// 重置内部状态
		resultMps->clear();
		// 设置新的
		convertToMPS(poly, resultMps);
	}
	void MBSOCore::setResultMPS(const std::vector<std::vector<MPoint_2>> &polyset) {
		// 回收边和点:结果集的元素，全部回收即可
		for (auto& mpoly : resultMps->mpolygons) {
			for (auto& edge : mpoly.edges)
				edgesMemoryPool.pushReuseElement(edge);
			for (auto& vertex : mpoly.vertexs)
				vertexsMemoryPool.pushReuseElement(vertex);
		}
		// 重置内部状态
		resultMps->clear();
		// 设置新的
		convertToMPS(polyset, resultMps);
	}

	void MBSOCore::intersect(const std::vector<MPoint_2>& poly) {
		// 改变指针
		MPolygonSet* tmp = mps1;
		mps1 = resultMps;
		convertToMPS(poly, mps2);
		resultMps = tmp;
		// 求解
		solve(mps1, mps2, resultMps, INTER);
	}
	void MBSOCore::intersect(const std::vector<std::vector<MPoint_2>>& polyset) {
		// 改变指针
		MPolygonSet* tmp = mps1;
		mps1 = resultMps;
		convertToMPS(polyset, mps2);
		resultMps = tmp;
		// 求解
		solve(mps1, mps2, resultMps, INTER);
	}

	void MBSOCore::join(const std::vector<MPoint_2>& poly) {
		// 改变指针
		MPolygonSet* tmp = mps1;
		mps1 = resultMps;
		convertToMPS(poly, mps2);
		resultMps = tmp;
		// 求解
		solve(mps1, mps2, resultMps, UNION);
	}
	void MBSOCore::join(const std::vector<std::vector<MPoint_2>>& polyset) {
		// 改变指针
		MPolygonSet* tmp = mps1;
		mps1 = resultMps;
		convertToMPS(polyset, mps2);
		resultMps = tmp;
		// 求解
		solve(mps1, mps2, resultMps, UNION);
	}

	void MBSOCore::difference(const std::vector<MPoint_2>& poly) {
		// 改变指针
		MPolygonSet* tmp = mps1;
		mps1 = resultMps;
		convertToMPS(poly, mps2);
		resultMps = tmp;
		// 求解
		solve(mps1, mps2, resultMps, DIFF);
	}
	void MBSOCore::difference(const std::vector<std::vector<MPoint_2>>& polyset) {
		// 改变指针
		MPolygonSet* tmp = mps1;
		mps1 = resultMps;
		convertToMPS(polyset, mps2);
		resultMps = tmp;
		// 求解
		solve(mps1, mps2, resultMps, DIFF);
	}
	void MBSOCore::difference(const std::vector<std::vector<MPoint_2>>& polyset, robin_hood::unordered_map<int, std::vector<int>>& po_cut_nodes) {
		// 改变指针
		MPolygonSet* tmp = mps1;
		mps1 = resultMps;
		convertToMPS(polyset, mps2);

		robin_hood::unordered_map<MPolygon*, int> mps2_poly_ptr_to_index; // 指针映射数组下标
		for (int i = 0; i < mps2->mpolygons.size(); ++i) {
			mps2_poly_ptr_to_index[&mps2->mpolygons[i]] = i;
		}

		resultMps = tmp;
		// 求解
		solve(mps1, mps2, resultMps, DIFF);

		for (int i = 0; i < resultMps->mpolygons.size(); ++i) {
			for (auto& edge : resultMps->mpolygons[i].edges) {
				if (edge->polygonSetId == 1) { // 是来自第二个多边形集的边
					int index = mps2_poly_ptr_to_index[edge->polygonPtr]; // 获取下标
					po_cut_nodes[index].push_back(i); //记录切割关系
				}
			}
		}
	}

	std::vector<std::vector<MPoint_2>> MBSOCore::getResult() {
		std::vector<std::vector<MPoint_2>> ans;
		int polygonSize = resultMps->getMPolygonSize();
		ans.resize(polygonSize);
		// 提取每个多边形
		for (int i = 0; i < polygonSize; ++i) {
			auto& ansPolygon = ans[i];
			ansPolygon.reserve(resultMps->mpolygons[i].edges.size());
			// 提取每个点
			const auto& edges = resultMps->mpolygons[i].edges;
			for (auto it = edges.rbegin(); it != edges.rend(); ++it) {
			    const auto& edgePtr = *it;
			    ansPolygon.emplace_back(edgePtr->ori->point);
			}
		}
		return ans;
	}

	/* --------------- 支持二元运算外部接口 --------------- */
	std::vector<std::vector<MPoint_2>> MBSOCore::solve(const std::vector<MPoint_2>& poly1, OP_TYPE opt, const std::vector<MPoint_2>& poly2) {
		setResultMPS(poly1);
		switch (opt)
		{
		case MBSO::INTER:
			intersect(poly2);
			break;
		case MBSO::UNION:
			join(poly2);
			break;
		case MBSO::DIFF:
			difference(poly2);
			break;
		default:
			throw std::logic_error(__func__ + std::string("未定义布尔操作"));
		}
		//返回结果
		return getResult();
	}
	std::vector<std::vector<MPoint_2>> MBSOCore::solve(const std::vector<std::vector<MPoint_2>>& polyset1, OP_TYPE opt, const std::vector<MPoint_2>& poly2) {
		setResultMPS(polyset1);
		switch (opt)
		{
		case MBSO::INTER:
			intersect(poly2);
			break;
		case MBSO::UNION:
			join(poly2);
			break;
		case MBSO::DIFF:
			difference(poly2);
			break;
		default:
			throw std::logic_error(__func__ + std::string("未定义布尔操作"));
		}
		//返回结果
		return getResult();
	}
	std::vector<std::vector<MPoint_2>> MBSOCore::solve(const std::vector<MPoint_2>& poly1, OP_TYPE opt, const std::vector<std::vector<MPoint_2>>& polyset2) {
		setResultMPS(poly1);
		switch (opt)
		{
		case MBSO::INTER:
			intersect(polyset2);
			break;
		case MBSO::UNION:
			join(polyset2);
			break;
		case MBSO::DIFF:
			difference(polyset2);
			break;
		default:
			throw std::logic_error(__func__ + std::string("未定义布尔操作"));
		}
		//返回结果
		return getResult();
	}
	std::vector<std::vector<MPoint_2>> MBSOCore::solve(const std::vector<std::vector<MPoint_2>>& polyset1, OP_TYPE opt, const std::vector<std::vector<MPoint_2>>& polyset2) {
		setResultMPS(polyset1);
		switch (opt)
		{
		case MBSO::INTER:
			intersect(polyset2);
			break;
		case MBSO::UNION:
			join(polyset2);
			break;
		case MBSO::DIFF:
			difference(polyset2);
			break;
		default:
			throw std::logic_error(__func__ + std::string("未定义布尔操作"));
		}
		//返回结果
		return getResult();
	}

	/* --------------- 将外部多边形表示转成内部多边形集, 外部传进来的是逆时针顺序 --------------- */
	void MBSOCore::convertToMPS(const std::vector<MPoint_2> & poly, MPolygonSet* & mps){
		if (mps == nullptr || mps->mpolygons.size() != 0) { // 传进来的 mps 一般情况只会是 重置状态后的 成员变量 mps1,mps2或resultMps
			std::logic_error(__func__ + std::string("本函数不负责显式内存管理，mps不应为空指针，请在外部申请好空间, mps内部不应存在多边形，请在外部释放"));
		}
		// 只需构造一个多边形
		mps->mpolygons.reserve(1);
		int _mpolySize = poly.size(); // 该多边形边数
		// 创建多边形
		MPolygon mpolygon;
		mpolygon.vertexs.reserve(_mpolySize);
		mpolygon.edges.reserve(_mpolySize);
		// 构造点集
		auto& vertexs = mpolygon.vertexs;
		for (auto it = poly.rbegin(); it != poly.rend(); ++it) { // 反向遍历，因为内部是顺时针顺序
		    const auto& _mpoint = *it;
		    MVertex* mvertex = vertexsMemoryPool.newElement(_mpoint);
		    vertexs.emplace_back(mvertex);
		}
		// 基于点集构造边集
		for (int i = 0; i < _mpolySize; ++i) {
			const auto& start = vertexs[i];
			size_t next = i + 1;
			if(next == _mpolySize) next = 0;
			const auto& end = vertexs[next];
			MEdge* medge = edgesMemoryPool.newElement(start, end);
			mpolygon.edges.emplace_back(medge);
		}
		// 放入多边形集中
		mps->mpolygons.emplace_back(std::move(mpolygon));
		// 初始化多边形集内部几何元素状态
		mps->isNeedResetStatus = true;
		mps->resetStatus();
	}
	void MBSOCore::convertToMPS(const std::vector<std::vector<MPoint_2>> & polyset, MPolygonSet* & mps){
		if (mps == nullptr || mps->mpolygons.size() != 0) { // 传进来的 mps 一般情况只会是 重置状态后的 成员变量 mps1,mps2或resultMps
			std::logic_error(__func__ + std::string("本函数不负责显式内存管理，mps不应为空指针，请在外部申请好空间, mps内部不应存在多边形，请在外部释放"));
		}
		// 依次构造每个多边形
		mps->mpolygons.reserve(polyset.size());
		for (const auto& _mpoly : polyset) {
			int _mpolySize = _mpoly.size(); // 该多边形边数
			// 创建多边形
			MPolygon mpolygon;
			mpolygon.vertexs.reserve(_mpolySize);
			mpolygon.edges.reserve(_mpolySize);
			// 构造点集
			auto& vertexs = mpolygon.vertexs;
			for (auto it = _mpoly.rbegin(); it != _mpoly.rend(); ++it) { // 反向遍历，因为内部是顺时针顺序
			    const auto& _mpoint = *it;
			    MVertex* mvertex = vertexsMemoryPool.newElement(_mpoint);
			    vertexs.emplace_back(mvertex);
			}
			// 基于点集构造边集
			for (int i = 0; i < _mpolySize; ++i) {
				const auto& start = vertexs[i];
				size_t next = i + 1;
				if(next == _mpolySize) next = 0;
				const auto& end = vertexs[next];
				MEdge* medge = edgesMemoryPool.newElement(start, end);
				mpolygon.edges.emplace_back(medge);
			}
			// 放入多边形集中
			mps->mpolygons.emplace_back(std::move(mpolygon));
		}
		// 初始化多边形集内部几何元素状态
		mps->isNeedResetStatus = true;
		mps->resetStatus();
	}


	void MBSOCore::solve(MPolygonSet* _mps1, MPolygonSet* _mps2, MPolygonSet* _resultMps, OP_TYPE _opt) {
		// 赋值
		mps1 = _mps1, mps2 = _mps2, resultMps = _resultMps, opt = _opt;
		// 判断是否需要进行布尔运算
		if (!isNeedSolve()) return;
		// 初始化工作
		initial();
		// 网格法求交点
		// caculateIntersBasedOnBlocks();
		caculateIntersBruteForce(); // 暴力求交点
		// 交点插入轮廓
		insertAnddealLR();
		// 处理嵌套
		dealIsolatedMPolygon();
		// 绕边
		crossEdges();
		// 内存回收
		memoryRecycle();
	}

	bool MBSOCore::isNeedSolve()
	{
		bool ret = true;
		if (mps1->edgeCnt == 0)
		{
			ret = false;
			if (opt == OP_TYPE::UNION && mps2->edgeCnt != 0)
			{
				resultMps->mpolygons = std::move(mps2->mpolygons);
				resultMps->edgeCnt = mps2->edgeCnt;
				resultMps->box = mps2->box;
			}
		}
		else if (mps2->edgeCnt == 0)
		{
			// mps1 != null
			ret = false;
			if (opt == OP_TYPE::DIFF || opt == OP_TYPE::UNION)
			{
				resultMps->mpolygons = std::move(mps1->mpolygons);
				resultMps->edgeCnt = mps1->edgeCnt;
				resultMps->box = mps1->box;
			}
		}
		else if (!mps1->box.isIntersects(mps2->box))
		{
			// 如果两个多边形的包围盒不发生重叠
			// 交集是空集，不用管
			ret = false;
			if (opt == OP_TYPE::UNION)
			{
				resultMps->mpolygons.reserve(mps1->mpolygons.size() + mps2->mpolygons.size());
				for (auto& outer : mps1->mpolygons) resultMps->mpolygons.emplace_back(outer);
				for (auto& outer : mps2->mpolygons) resultMps->mpolygons.emplace_back(outer);
				resultMps->edgeCnt = mps1->edgeCnt + mps2->edgeCnt;
				resultMps->box = box;
			}
			else if (opt == OP_TYPE::DIFF)
			{
				resultMps->mpolygons = std::move(mps1->mpolygons);
				resultMps->edgeCnt = mps1->edgeCnt;
				resultMps->box = mps1->box;
			}
		}
		return ret;
	}

	inline void MBSOCore::initial() {
		// 输入多边形集信息初始化
		mps1->resetStatus();
		mps2->resetStatus();
		mps1->setPolygonSetId(0); // 令mps1的polygonSetId = 0, 初始化多边形集
		mps2->setPolygonSetId(1); // 令mps2的polygonSetId = 1

		// 重置结果容器
		int totalPolygonSize = mps1->getMPolygonSize() + mps2->getMPolygonSize();
		resultMps->clear();
		resultMps->mpolygons.reserve(totalPolygonSize);

		// 更新包围盒
		box.reset();
		blkBox.reset();
		box.update(mps1->box);
		box.update(mps2->box);

		// 重置网格状态
		grid.clear();
		blockWidth = 0;
		blockHeight = 0;
		blockCount = 0;

		// 重置辅助数据结构
		isolatedBeContained.clear();
		beCollided.clear();
		isolated1.clear();
		isolated2.clear();
		isolatedBeContained.reserve(totalPolygonSize);
		beCollided.reserve(totalPolygonSize);
		isolated1.reserve(totalPolygonSize);
		isolated2.reserve(totalPolygonSize);
		inPoints.clear();
		outPoints.clear();
		inPointsIndex = outPointsIndex = 0;

		hasInterEdges.clear();
		int allSegsSize = mps1->edgeCnt + mps2->edgeCnt;
		curOuter.clear();
		curOuter.reserve(allSegsSize);

		// 内存复用相关
		newVertexs.clear();
		newEdges.clear();
		newVertexs.reserve(allSegsSize);
		newEdges.reserve(allSegsSize);
	}

	inline void MBSOCore::caculateIntersBruteForce(){
		// 收集多边形包围盒位置信息
		for (int polygonSetId = 0; polygonSetId <= 1; ++polygonSetId)
		{
			auto& mps = (polygonSetId == 0 ? mps1 : mps2);
			auto& isolatedcurr = (polygonSetId == 0 ? isolated1 : isolated2);

			auto& mpolygons = mps->mpolygons;
			for (auto& polygon : mpolygons) {
				if (polygon.box.isBeContained(blkBox)) { // 多边形完全在交集包围盒区域内
					isolatedBeContained.emplace_back(&polygon);
				}
				if (polygon.box.isIntersects(blkBox)) {  // 多边形与交集包围盒区域有重叠
					beCollided.emplace_back(&polygon);
				}
				isolatedcurr.emplace_back(&polygon);
			}
		}

		// 暴力求交点
		for(auto& mpolya : mps1->mpolygons){
			for(auto& mpolyb : mps2->mpolygons){
				for(auto& seg1 : mpolya.edges){
					for(auto& seg2 : mpolyb.edges){
						if (caculateInterSSself(seg1, seg2)){
							hasInterEdges.insert(seg1);
							hasInterEdges.insert(seg2);
						}
					}
				}
			}
		}
	}

	inline void MBSOCore::caculateIntersBasedOnBlocks() {
		chooseBlkCntBasedOnInterRegion();
		// 遍历碰撞的多边形，找出所有轮廓边经过的格子
		for (auto& outer : beCollided) {
			int outerSize = outer->edges.size();
			// 遍历轮廓边
			for (int i = 0; i < outerSize; ++i) {
				auto& edge = outer->edges[i];
				// 找出该边经过的所有交集区域内的网格
				findEdgePassedBlockOnInterRegion(edge);
			}
		}

		std::unordered_set <pair<MEdge*, MEdge*>, pair_hash> calculated;
		// 遍历所有的格子，然后遍历格子里面的所有线段 [csg]?直接只遍历有线段的格子竟然更慢
		// for(auto& pa:grid.usedId)
		// int i = pa.first;
		// int j = pa.second;
		for (int i = 0; i <= blockCount; ++i)
		{
			for (int j = 0; j <= blockCount; ++j)
			{
				if (grid.grid[i][j].size() <= 1) continue;
				int n = grid.grid[i][j].size();
				for (int ii = 0; ii < n; ++ii)
				{
					auto& seg1 = grid.grid[i][j][ii];
					for (int jj = ii + 1; jj < n; ++jj)
					{
						auto& seg2 = grid.grid[i][j][jj];
						if (seg1 == seg2) continue; // 同一条边不求交
						if (calculated.count({ seg1,  seg2 })) continue; // 之前的网格中已经计算过了该组合
						if (seg1->polygonSetId == seg2->polygonSetId) continue; // 同属于一个多边形集的边不求交
						calculated.insert({ seg1, seg2 });
						calculated.insert({ seg2, seg1 });

						// 同一网格内的两条边尝试求交点
						if (caculateInterSSself(seg1, seg2))
						{
							hasInterEdges.insert(seg1);
							hasInterEdges.insert(seg2);
						}
					}
				}
			}
		}
	}

	void MBSOCore::chooseBlkCntBasedOnInterRegion() {
		// 获取他们的交集区域包围盒
		blkBox = mps1->box.getInterBox(mps2->box);

		if (blkBox.minX == blkBox.maxX) {
			blkBox.minX -= 1;
			blkBox.maxX += 1;
		}
		if (blkBox.minY == blkBox.maxY) {
			blkBox.minY -= 1;
			blkBox.maxY += 1;
		}

		// 自适应网格
		double polygonCnt = 0;
		double avgLength = 0;

		// 收集多边形包围盒位置信息
		for (int polygonSetId = 0; polygonSetId <= 1; ++polygonSetId)
		{
			auto& mps = (polygonSetId == 0 ? mps1 : mps2);
			auto& isolatedcurr = (polygonSetId == 0 ? isolated1 : isolated2);

			auto& mpolygons = mps->mpolygons;
			for (auto& polygon : mpolygons) {
				polygon.polygonSetId = polygonSetId;
				if (polygon.box.isBeContained(blkBox)) { // 多边形完全在交集包围盒区域内
					isolatedBeContained.emplace_back(&polygon);
				}
				if (polygon.box.isIntersects(blkBox)) {  // 多边形与交集包围盒区域有重叠
					beCollided.emplace_back(&polygon);
					avgLength += polygon.avgLength;
					polygonCnt++;
				}
				isolatedcurr.emplace_back(&polygon);
			}
		}

		avgLength /= polygonCnt;

		int t = ceil((blkBox.maxX - blkBox.minX) / avgLength);
		blockCount = min(max(t, 5), 100);
		blockHeight = static_cast<double>(blkBox.maxY - blkBox.minY) / blockCount;
		blockWidth = static_cast<double>(blkBox.maxX - blkBox.minX) / blockCount;
		inPoints.reserve(beCollided.size() * 10);
		outPoints.reserve(beCollided.size() * 10);
	}

	inline void MBSOCore::findEdgePassedBlockOnInterRegion(MEdge* v) {
		// 曼哈顿边必然是水平或垂直的
		Bbox v_box;
		v_box.update(v->ori->point), v_box.update(v->dest->point);
		// 若边在交集区域包围盒外，则跳过
		if (!blkBox.isIntersects(v_box)) return;

		if (v->seg.is_horizontal) { // 水平边：Y固定，遍历X方向网格
			// 获取边与包围盒区域的交集
			int xmin = max(v_box.minX, blkBox.minX);
			int xmax = min(v_box.maxX, blkBox.maxX);

			int blockYId = getBlockYId(v_box.minY);
			int startBlockXId = getBlockXId(xmin);
			int endBlockXId = getBlockXId(xmax);
			for (int xId = startBlockXId; xId <= endBlockXId; ++xId) {
				grid.emplace_back(xId, blockYId, v);
			}

		}
		else { // 垂直边：X固定，遍历Y方向网格
			// 获取边与包围盒区域的交集
			int ymin = max(v_box.minY, blkBox.minY);
			int ymax = min(v_box.maxY, blkBox.maxY);

			int blockXId = getBlockXId(v_box.minX);
			int startBlockYId = getBlockYId(ymin);
			int endBlockYId = getBlockYId(ymax);
			for (int yId = startBlockYId; yId <= endBlockYId; ++yId) {
				grid.emplace_back(blockXId, yId, v);
			}
		}
	}

	bool MBSOCore::caculateInterSSself(MEdge* seg1, MEdge* seg2)
	{
		MPoint_2 p, q;
		POINT_TYPE ret;
		// 判是否相交和求交
		ret = seg1->seg.isSegCrossSegAndInter(seg2->seg, p, q);

		if (ret == NOT_INTER) return false;			// 没有交点
		else if (ret == INTER_POINT) {
			dealSSInterWhenPoint(seg1, seg2, p);	// 有一个交点
		}
		else if (ret == INTER_SEG) {				// 有两个交点
			dealSSInterWhenPoint(seg1, seg2, p);
			dealSSInterWhenPoint(seg1, seg2, q);
		}
		return true;
	}

	void MBSOCore::dealSSInterWhenPoint(MEdge* seg1, MEdge* seg2, MPoint_2& inter_p) {
		if (seg1->polygonSetId != 0) std::swap(seg1, seg2);	// 保证seg1指向A，seg2指向B
		int equalPointsSize = 0;
		MEdge* anotherSeg = nullptr; // 只有当 equalPointsSize = 1 时有用
		// 统计
		if (seg1->ori->point == inter_p) equalPoints[equalPointsSize++] = seg1->ori, anotherSeg = seg2;
		else if (seg1->dest->point == inter_p) equalPoints[equalPointsSize++] = seg1->dest, anotherSeg = seg2;
		if (seg2->ori->point == inter_p) equalPoints[equalPointsSize++] = seg2->ori, anotherSeg = seg1;
		else if (seg2->dest->point == inter_p) equalPoints[equalPointsSize++] = seg2->dest, anotherSeg = seg1;

		// 根据交点位置分类讨论
		if (equalPointsSize == 2)
		{
			// 交点在两条线段的端点处
			MVertex* p1 = equalPoints[0], * p2 = equalPoints[1];
			if (p1->isInter && p2->isInter) return;
			p1->interP = p2->interP = inter_p;
			p1->pointType = p2->pointType = POINT_POINT;
			p1->isInter = p2->isInter = true;
			p1->anotherVertex = p2, p2->anotherVertex = p1;
		}
		else if (equalPointsSize == 1)
		{
			// 交点在一条线段的端点处
			MVertex* p1 = equalPoints[0];
			if (p1->isInter) return;
			p1->pointType = POINT_SEG;
			p1->isInter = true;
			p1->anotherVertex = anotherSeg->ori; // 主要是为了标记轮廓存在交点
			anotherSeg->push_interPoint(p1);
		}
		else if (equalPointsSize == 0)
		{
			// 交点不在端点处
			// 新建交点
			MVertex* p1 = vertexsMemoryPool.newElement(inter_p);
			newVertexs.emplace_back(p1);
			p1->isInter = true;
			p1->pointType = SEG_SEG;
			p1->isFinished = true;

			seg1->push_interPoint(p1);
			seg2->push_interPoint(p1);

			// 计算叉积符号，确定出入点类型
			MPoint_2 v1 = seg1->seg.vSeg;
			MPoint_2 v2 = seg2->seg.vSeg;
			double ret = v1 ^ v2;
			if (ret > 0) p1->interType = OUT_POINT;
			else if (ret < 0) p1->interType = IN_POINT;

			pushBackInterPoint(p1);

			// 将这两条直线所在的轮廓打上不孤立存在的标记
			seg1->polygonPtr->existInterPoint = true;
			seg2->polygonPtr->existInterPoint = true;
		}
	}

	void MBSOCore::insertAnddealLR()
	{
		MVertex* cur = nullptr, * pre = nullptr;
		// 先把所有的交点插入到线段中
		// 遍历所有有交点的线段
		for (auto& seg : hasInterEdges)
		{
			auto& interPs = seg->interPoints;
			// 如果没有交点，跳过
			if (interPs.size() == 0) continue;
			// 交点大于等于2则先按交点到边起点的距离从小到大排序
			if (interPs.size() >= 2) {
				sort(interPs.begin(), interPs.end(), [&](const MVertex* x, const MVertex* y) {return seg->seg.distanceFromOri(x->point) < seg->seg.distanceFromOri(y->point); });
			}
			// 开始插入
			pre = seg->ori;
			for (auto& node : interPs)
			{
				cur = node;
				// 生成新边
				MEdge* curSeg = edgesMemoryPool.newElement(pre, cur, seg->polygonSetId, seg->polygonPtr);
				newEdges.emplace_back(curSeg);

				// 连接拓扑
				if (seg->polygonSetId == 0) // 属于多边形集A
					pre->nextEdgeA = curSeg, cur->frontEdgeA = curSeg;
				else
					pre->nextEdgeB = curSeg, cur->frontEdgeB = curSeg;
				pre = cur;
			}

			// 最后一条边
			MEdge* curSeg = edgesMemoryPool.newElement(pre, seg->dest, seg->polygonSetId, seg->polygonPtr);
			newEdges.emplace_back(curSeg);
			if (seg->polygonSetId == 0) pre->nextEdgeA = curSeg, seg->dest->frontEdgeA = curSeg;
			else pre->nextEdgeB = curSeg, seg->dest->frontEdgeB = curSeg;
			seg->resetFlags(); // 该边已经被拆分完了，重置内部状态(释放存储的交点)，这条边拆完后其实已经没用了，后续会在回收原多边形未复用边时被回收的
		}

		// 处理端点相交的情况
		for (auto& seg : hasInterEdges)
		{
			for (int i = 0; i <= 1; ++i)
			{
				auto& cur = (i == 0 ? seg->ori : seg->dest);
				if (!cur->isInter || cur->isFinished) continue;
				// 是交点，判断交点类型
				if (cur->pointType == POINT_POINT) // 两个端点相交
				{
					splitPoint(cur);
				}
				else if (cur->pointType == POINT_SEG) // 一个端点和一个线段相交
				{
					splitPoint(cur);
				}
			}
		}
	}

	/****************************************
	ver2.1.1 新的拆点策略，利用求和之后的属性
	首先求出来该点的 几何数和 属性，如果是0则考虑拆点，不为0的话直接不拆
	先假设已经拆开，如果拆开的两个点都是0，则最终不拆，而且是0
	两个点不同时为0，则真正的拆开。
	*/
	void MBSOCore::splitPoint(MVertex* point)
	{
		MVertex* cur = point, * another = nullptr;
		cur->isFinished = true;
		if (point->pointType == POINT_POINT)
		{
			another = point->anotherVertex;
			another->isFinished = true;

			// TODO:: 可能有问题，修改顶点坐标为交点坐标 [csg?]原来的注释，暂未细看
			cur->point = another->point = cur->interP;
			if (cur->polygonSetId != 0) std::swap(cur, another); // 保证 cur 为指向 多边形集A 上的点

			cur->frontEdgeB = another->frontEdgeB;
			cur->nextEdgeB = another->nextEdgeB;
			another->frontEdgeB->dest = cur;
			another->nextEdgeB->ori = cur;
			//!!! another为指向 多边形集B 上的端点，上面的操作直接修改了B的边指针,虽然通过其边数组已访问不到another这个点，但是原多边形点数组能访问到，还是可以正常内存回收
		}
		// 假设 cur 在 polygonSetId = 0 上，为了插入方便，首先定义这些
		MPoint_2 curFrontPtB, curNextPtB, curFrontPtA, curNextPtA;
		curFrontPtB = cur->frontEdgeB->ori->point;
		curNextPtB = cur->nextEdgeB->dest->point;
		curFrontPtA = cur->frontEdgeA->ori->point;
		curNextPtA = cur->nextEdgeA->dest->point;
		// 向量
		MPoint_2 vFrontPtACur = (cur->point - curFrontPtA);		// curFrontPtA --> cur;
		MPoint_2 vCurNextPtA = (curNextPtA - cur->point);		// cur --> curNextPtA;
		MPoint_2 vFrontPtBCur = (cur->point - curFrontPtB);		// curFrontPtB --> cur;
		MPoint_2 vCurNextPtB = (curNextPtB - cur->point);		// cur --> curNextPtB;

		// 等于 0，需要拆点，首先模拟拆点
		// 找到inB，outB中和 inA 最近的，和 outA 最近的，然后判断哪个更近
		MPoint_2 tmpPtA, tmpPtB, minPtA, minPtB;
		double minAngle = INF;
		for (int i = 0; i < 2; ++i)
		{
			tmpPtA = (i == 0 ? curFrontPtA : curNextPtA);
			for (int j = 0; j < 2; ++j)
			{
				tmpPtB = (j == 0 ? curFrontPtB : curNextPtB);
				double angle = cur->point.rad(tmpPtA, tmpPtB);
				if (angle < minAngle)
				{
					minAngle = angle;
					minPtA = tmpPtA;
					minPtB = tmpPtB;
				}
			}
		}
		int crossCur, crossP;
		// 判断拆开后的两个点的属性
		// 先判断原来的 cur 点
		MPoint_2 vInA, vOutA, vInB, vOutB;
		if (minPtA == cur->frontEdgeA->ori->point)
		{
			vInA = vFrontPtACur, vOutA = vCurNextPtA;
			if (minPtB == cur->frontEdgeB->ori->point) vInB = vInA, vOutB = vCurNextPtB;
			else vInB = vFrontPtBCur, vOutB = -vInA;
		}
		else
		{
			vInA = vFrontPtACur, vOutA = vCurNextPtA;
			if (minPtB == cur->frontEdgeB->ori->point) vInB = -vOutA, vOutB = vCurNextPtB;
			else vInB = vFrontPtBCur, vOutB = vOutA;
		}
		crossCur = sgn(sgn(vInA ^ vInB, eps_double) + sgn(vOutA ^ vInB, eps_double) + sgn(vInA ^ vOutB, eps_double) + sgn(vOutA ^ vOutB, eps_double), eps_double);

		// 再判断后面的 p 点
		if (minPtA == cur->frontEdgeA->ori->point)
		{
			vInA = vFrontPtACur, vOutA = vInA;
			if (minPtB == cur->frontEdgeB->ori->point) vInB = vFrontPtBCur, vOutB = vOutA;
			else vInB = -vOutA, vOutB = vCurNextPtB;
		}
		else
		{
			vInA = vCurNextPtA, vOutA = vInA;
			if (minPtB == cur->frontEdgeB->ori->point) vInB = vFrontPtBCur, vOutB = -vInA;
			else vInB = vInA, vOutB = vCurNextPtB;
		}
		crossP = sgn(sgn(vInA ^ vInB, eps_double) + sgn(vOutA ^ vInB, eps_double) + sgn(vInA ^ vOutB, eps_double) + sgn(vOutA ^ vOutB, eps_double), eps_double);

		if (crossCur == 0 && crossP == 0)
		{
			cur->interType = UNKNOWN;
			cur->isInter = false;
			//if (cur->pointType == POINT_POINT) {
			//	cur->frontEdgeB = cur->frontEdgeA;
			//	cur->nextEdgeB = cur->nextEdgeA;
			//	another->frontEdgeB->dest = another;
			//	another->nextEdgeB->ori = cur;
			//}
			// 之前修改了B的边指针
			return;
		}
		else if (crossCur + crossP != 0)
		{
			cur->interType = (crossCur + crossP > 0 ? OUT_POINT : IN_POINT);

			pushBackInterPoint(cur);

			// 这一种情况和下面都应该给轮廓打上标记
			// 将这两条直线所在的轮廓打上不孤立存在的标记
			cur = point;
			another = cur->anotherVertex;
			if (cur->polygonSetId != 0) std::swap(cur, another);
			cur->polygonPtr->existInterPoint = true;
			another->polygonPtr->existInterPoint = true;
			return;
		}
		// 两个不相等，因此只能拆点了，并且一正一负
		MVertex* p = vertexsMemoryPool.newElement(*cur);
		newVertexs.emplace_back(p);
		cur->interType = (crossCur > 0 ? OUT_POINT : IN_POINT);
		p->interType = (crossP > 0 ? OUT_POINT : IN_POINT);

		pushBackInterPoint(p);
		pushBackInterPoint(cur);

		// 需要创建两条边，
		MEdge* segA = edgesMemoryPool.newElement();
		MEdge* segB = edgesMemoryPool.newElement();
		newEdges.emplace_back(segA);
		newEdges.emplace_back(segB);
		MVertex* pre = nullptr, * next = nullptr;
		// 在 A 中插入 p;
		if (minPtA == cur->frontEdgeA->ori->point) {	// cur 之前插入
			cur->frontEdgeA->dest = p;
			segA->setOriDest(p, cur);
			p->nextEdgeA = cur->frontEdgeA = segA;
			//pre = cur->frontEdgeA->ori, next = cur;
		}
		else {									// cur 之后插入
			cur->nextEdgeA->ori = p;
			segA->setOriDest(cur, p);
			p->frontEdgeA = cur->nextEdgeA = segA;
			//pre = cur, next = cur->nextEdgeA->dest;
		}
		// 在 B 中插入 p;
		if (minPtB == cur->frontEdgeB->ori->point) {	// 在 cur 之前插入
			cur->frontEdgeB->dest = p;
			segB->setOriDest(p, cur);
			p->nextEdgeB = cur->frontEdgeB = segB;
			//pre = cur->frontEdgeB, next = cur;
		}
		else {									// 在 cur 之后插入
			cur->nextEdgeB->ori = p;
			segB->setOriDest(cur, p);
			p->frontEdgeB = cur->nextEdgeB = segB;
		}
		cur = point;
		another = cur->anotherVertex;
		if (cur->polygonSetId != 0) std::swap(cur, another);
		cur->polygonPtr->existInterPoint = true;
		another->polygonPtr->existInterPoint = true;
	}

	/****************************************
	孤立轮廓解决方案：处理孤立轮廓应该在绕边之前
	1.位于交集区域外的孤立轮廓，直接根据运算类型确定是否保留孤立轮廓
	2.位于交集区域内的孤立轮廓，若它是A,则遍历B中所有轮廓，找出嵌套它的轮廓
	  若它是B,则遍历A中所有轮廓，找出嵌套它的轮廓
	2.1并集：A的，被B的包含，丢弃；找不到即未被包含，保留
			 B的，被A的包含，丢弃；找不到即未被包含，保留
	2.2交集：A的，被B的包含，保留；找不到即未被包含，丢弃
			 B的，被A的包含，保留；找不到即未被包含，丢弃
	2.3差集：A的，被B的包含，丢弃；找不到即未被包含，保留
			 B的，被A的包含(会出现孔洞，暂不考虑有洞的情形)，丢弃；找不到即未被包含，丢弃
	*/
	void MBSOCore::dealIsolatedMPolygon()
	{
		vector<MEdge*> isolatedSegs;			// 存孤立轮廓的点
		int isolated_size = isolatedBeContained.size();
		if (isolated_size == 0) // 没有位于交集区域内的孤立轮廓，直接根据运算类型确定是否保留孤立轮廓
		{
			for (auto* outer : isolated1)
			{
				if (outer->existInterPoint) continue;
				if (opt == UNION) pushBackToResultMps(*outer);
				else if (opt == DIFF) pushBackToResultMps(*outer);
			}
			for (auto* outer : isolated2)
			{
				if (outer->existInterPoint) continue;
				if (opt == UNION) pushBackToResultMps(*outer);
			}
			return;
		}

		// 位于交集区域内的孤立轮廓
		for (auto* outer : isolatedBeContained)
		{
			if (outer->existInterPoint) continue;
			if (outer->isNested) continue;

			// 先找出来可能可以嵌套他的轮廓
			MVertex* right = nullptr;
			auto& anotherIsolated = (outer->polygonSetId == 0 ? isolated2 : isolated1);
			int anotherIsolatedSize = anotherIsolated.size();
			for (int j = 0; j < anotherIsolatedSize; ++j)
			{
				int polygonSetId = anotherIsolated[j]->polygonSetId;
				if (!outer->box.isBeContained(anotherIsolated[j]->box)) continue;
				//检测多边形outer的任意一个顶点是否在多边形anotherIsolated[j]内部, 是则能发生嵌套
				if (anotherIsolated[j]->isInside(outer->edges[0]->ori->point))
				{
					if (opt == UNION)
					{
						// 丢弃
					}
					else if (opt == INTER)
					{
						// 保留
						pushBackToResultMps(*outer);
					}
					else if (opt == DIFF)
					{
						// 丢弃或产生孔洞，暂不考虑有洞的情形
					}
					// 打标记，找到嵌套关系了
					outer->isNested = true;
					break;
				}
			}
		}
		//继续处理没有找到嵌套关系的孤立轮廓
		for (auto* outer : isolated1) // A的
		{
			if (outer->existInterPoint) continue; // 指非孤立则跳过
			if (outer->isNested) continue;		  // 指已经找到嵌套关系的跳过
			// 找不到所以未确定嵌套关系的
			if (opt == UNION) pushBackToResultMps(*outer);
			else if (opt == DIFF) pushBackToResultMps(*outer);
		}
		for (auto* outer : isolated2) // B的
		{
			if (outer->existInterPoint) continue; // 指非孤立则跳过
			if (outer->isNested) continue;		  // 指已经找到嵌套关系的跳过
			// 找不到所以未确定嵌套关系的
			if (opt == UNION) pushBackToResultMps(*outer);
		}
	}

	// 传入 mps1 轮廓上的起点（即从多边形A开始）
	void MBSOCore::crossEdges()
	{
		MVertex* curPt = nullptr;
		MVertex* s = nullptr;
		MEdge* curSeg = nullptr;
		curMps = 0;
		if (opt == OP_TYPE::UNION) {
			bool firstCross = true;
			INTER_TYPE nextInterType = INTER_TYPE::OUT_POINT;
			while (true) {	// 直接死循环
				firstCross = false;
				curMps = 0;
				curOuter.clear();
				// 找到第一个没被访问的交点，标记为 S
				curPt = findFirstInter(nextInterType);
				if (curPt == nullptr) break;
				if (!curPt->isInter || curPt->interType != nextInterType) continue;
				s = curPt;
				firstCross = true;
				while (curPt != s || firstCross) {
					firstCross = false;
					while (curPt->interType == nextInterType)
					{
						curPt->isVisted = true;
						curMps = (curMps == 0 ? 1 : 0);
						nextInterType = (nextInterType == OUT_POINT ? IN_POINT : OUT_POINT);
						curSeg = findNextEdge(curPt);
						if (curOuter.empty() || curSeg->seg.getLength() != 0)
							curOuter.emplace_back(curSeg);
						curPt = curSeg->dest;
						if (curPt == s || curPt->isVisted) break;
					}
					if (curPt == s || curPt->isVisted) break;
					while (curPt->interType != nextInterType)
					{
						curPt->isVisted = true;
						curSeg = findNextEdge(curPt);
						if (curOuter.empty() || curSeg->seg.getLength() != 0)
							curOuter.emplace_back(curSeg);
						curPt = curSeg->dest;
						if (curPt == s || curPt->isVisted) break;
					}
					if (curPt == s || curPt->isVisted) break;
				}
				if (curPt == s)
					pushBackToResultMps(curOuter);
				else if (curPt->isVisted)
					interceptOuter(curOuter, curPt);
			}
		}
		if (opt == OP_TYPE::INTER) {
			bool firstCross = true;
			while (true) {
				INTER_TYPE nextInterType = INTER_TYPE::IN_POINT;
				// 找到第一个交点
				curMps = 0;
				curOuter.clear();
				curPt = findFirstInter(nextInterType);
				if (curPt == nullptr) break;
				if (!curPt->isInter || curPt->interType != nextInterType) continue;
				s = curPt;
				firstCross = true;
				while (curPt != s || firstCross) {
					firstCross = false;
					while (curPt->interType == nextInterType)
					{
						curPt->isVisted = true;
						curMps = (curMps == 0 ? 1 : 0);
						nextInterType = (nextInterType == OUT_POINT ? IN_POINT : OUT_POINT);
						curSeg = findNextEdge(curPt);
						if (curOuter.empty() || curSeg->seg.getLength() != 0)
							curOuter.emplace_back(curSeg);
						curPt = curSeg->dest;
						if (curPt == s || curPt->isVisted) break;
					}
					if (curPt->isVisted || curPt == s) break;
					while (curPt->interType != nextInterType)
					{
						curPt->isVisted = true;
						curSeg = findNextEdge(curPt);
						if (curOuter.empty() || curSeg->seg.getLength() != 0)
							curOuter.emplace_back(curSeg);
						curPt = curSeg->dest;
						if (curPt == s || curPt->isVisted) break;
					}
					if (curPt == s || curPt->isVisted) break;
				}
				if (curPt == s)
					pushBackToResultMps(curOuter);
				else if (curPt->isVisted)
					interceptOuter(curOuter, curPt);
			}
		}
		if (opt == OP_TYPE::DIFF) {
			// A - B，A的顺时针，B的逆时针，找A的入点，换到B，找出点
			// 因为B是逆时针所以其实是找出点，入点。
			curPt = nullptr;
			bool firstCross = true;
			while (true) {
				// 找到第一个出点记为 s
				curMps = 0;
				curOuter.clear();
				INTER_TYPE nextInterType = INTER_TYPE::OUT_POINT;
				curPt = findFirstInter(nextInterType);
				if (curPt == nullptr) break;
				if (!curPt->isInter || curPt->interType != nextInterType) continue;
				s = curPt;
				firstCross = true;
				while (curPt != s || firstCross)
				{
					firstCross = false;
					while (curPt->interType == nextInterType)
					{
						curPt->isVisted = true;
						curMps = (curMps == 0 ? 1 : 0);
						nextInterType = (nextInterType == OUT_POINT ? IN_POINT : OUT_POINT);
						if (curMps == 1) {
							curSeg = findFrontEdge(curPt);
							curPt = curSeg->ori;
						}
						else {
							curSeg = findNextEdge(curPt);
							curPt = curSeg->dest;
						}
						if (curOuter.empty() || curSeg->seg.getLength() != 0)
							curOuter.emplace_back(curSeg);
						if (curPt == s || curPt->isVisted) break;
					}
					if (curPt->isVisted || curPt == s) break;
					while (curPt->interType != nextInterType)
					{
						curPt->isVisted = true;
						if (curMps == 1) {
							curSeg = findFrontEdge(curPt);
							curPt = curSeg->ori;
						}
						else {
							curSeg = findNextEdge(curPt);
							curPt = curSeg->dest;
						}
						if (curOuter.empty() || curSeg->seg.getLength() != 0)
							curOuter.emplace_back(curSeg);
						if (curPt == s || curPt->isVisted) break;
					}
					if (curPt == s || curPt->isVisted) break;
				}
				if (curPt == s)
					pushBackToResultMps(curOuter);
				else if (curPt->isVisted)
					interceptOuter(curOuter, curPt);
			}
		}
	}

	// 如果还存在带尾巴的情况，直接强行截断，
	void MBSOCore::interceptOuter(vector<MEdge*>& outer, MVertex* start)
	{
		int index = -1;
		int n = outer.size();
		for (int i = 0; i < n; ++i)
		{
			if (outer[i]->polygonSetId == 1 && opt == DIFF)
			{
				if (outer[i]->dest == start)
				{
					index = i;
					break;
				}
			}
			else
			{
				if (outer[i]->ori == start)
				{
					index = i;
					break;
				}
			}
		}
		if (index != -1)
		{
			vector<MEdge*> tmp(outer.begin() + index, outer.end());
			pushBackToResultMps(tmp);
		}
	}

	void MBSOCore::memoryRecycle()
	{
		/* 多边形通过冗余的独立存储拓扑边和点数组，内存回收直接遍历检查即可，不用担心重回收而导致bug */

		// 回收mps1
		for (auto& mpoly : mps1->mpolygons)
		{
			// 若是被结果复用了的多边形，跳过
			if (mpoly.isResultRecycle) continue;
			// 否则遍历边列表
			for (auto& edge : mpoly.edges)
				// 如果边未被复用，回收边
				if (!edge->isResultRecycle)
					edgesMemoryPool.pushReuseElement(edge);
			// 再遍历点列表
			for (auto& vertex : mpoly.vertexs)
				// 如果点未被复用，回收点
				if (!vertex->isResultRecycle)
					vertexsMemoryPool.pushReuseElement(vertex);
		}
		mps1->clear();

		// 回收mps2
		for (auto& mpoly : mps2->mpolygons)
		{
			if (mpoly.isResultRecycle) continue;
			for (auto& edge : mpoly.edges)
				if (!edge->isResultRecycle)
					edgesMemoryPool.pushReuseElement(edge);
			for (auto& vertex : mpoly.vertexs)
				if (!vertex->isResultRecycle)
					vertexsMemoryPool.pushReuseElement(vertex);
		}
		mps2->clear();

		//置结果集初始化标志位，这样如果还要用它参与下次运算，则其内部几何元素的状态才会在下次运算前于 initial() 函数被初始化
		resultMps->isNeedResetStatus = true;

		// 回收未被复用的新边
		for (auto& edge : newEdges)
			if (!edge->isResultRecycle)
				edgesMemoryPool.pushReuseElement(edge);

		// 回收未被复用的新点
		for (auto& vertex : newVertexs)
			if (!vertex->isResultRecycle)
				vertexsMemoryPool.pushReuseElement(vertex);
	}

} // namespace MBSO