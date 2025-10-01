#include "MEdge.h"

namespace MBSO {

	MEdge::MEdge() : ori(nullptr), dest(nullptr), polygonSetId(-1), polygonPtr(nullptr), isResultRecycle(false)
	{
	}

	MEdge::MEdge(MVertex* s, MVertex* e) : ori(s), dest(e), polygonSetId(-1), polygonPtr(nullptr), seg(s->point, e->point), isResultRecycle(false)
	{
	}

	void MEdge::resetAll()
	{
		ori = nullptr;
		dest = nullptr;
		polygonSetId = -1;
		polygonPtr = nullptr;
		seg = MSegment_2();
		interPoints.clear();
		isResultRecycle = false;
	}

	void MEdge::resetFlags()
	{
		seg.setSeg(ori->point, dest->point);
		interPoints.clear();
		isResultRecycle = false;
	}

	void MEdge::reverse()
	{
		std::swap(ori, dest);
		seg.reverse();
	}

	void MEdge::setOriDest(MVertex* _ori, MVertex* _dest, int _polygonSetId)
	{
		ori = _ori;
		dest = _dest;
		polygonSetId = _polygonSetId;
		seg.setSeg(ori->point, dest->point);
	}

	void MEdge::push_interPoint(MVertex* p)
	{
		interPoints.emplace_back(p);
	}
} // namespace MBSO
