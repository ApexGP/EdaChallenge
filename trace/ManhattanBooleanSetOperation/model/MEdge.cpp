#include "MEdge.h"

namespace MBSO {

	MEdge::MEdge() : ori(nullptr), dest(nullptr), polygonSetId(-1), polygonPtr(nullptr)
	{
	}

	MEdge::MEdge(MVertex* s, MVertex* e) : ori(s), dest(e), polygonSetId(-1), polygonPtr(nullptr), seg(s->point, e->point)
	{
	}

	void MEdge::init()
	{
		seg.setSeg(ori->point, dest->point);
		interPoints.clear();
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
