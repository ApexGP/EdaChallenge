#pragma once
#include "../const/Const.h"
#include "Point_2.h"
#include <algorithm> // Include this header for std::min and std::max

namespace MBSO
{
	// Axis Aligned Bounding Box
	class Bbox
	{
	public:
		int minX;
		int minY;
		int maxX;
		int maxY;

		Bbox() :minX(INT_MAX), minY(INT_MAX), maxX(INT_MIN), maxY(INT_MIN) {}

		Bbox(int _minx, int _miny, int _maxx, int _maxy) :
			minX(_minx), minY(_miny), maxX(_maxx), maxY(_maxy){}

		Bbox(const Bbox& p) : minX(p.minX), minY(p.minY), maxX(p.maxX), maxY(p.maxY) {}
	
		void reset()
		{
			minX = minY = INT_MAX;
			maxX = maxY = INT_MIN;
		}
		void update(const Point_2& p)
		{
			minX = std::min(minX, p.getX());
			minY = std::min(minY, p.getY());
			maxX = std::max(maxX, p.getX());
			maxY = std::max(maxY, p.getY());
		}
		void update(const Bbox& p)
		{
			minX = std::min(minX, p.minX);
			minY = std::min(minY, p.minY);
			maxX = std::max(maxX, p.maxX);
			maxY = std::max(maxY, p.maxY);
		}

		// 检查两个包围盒是否相交
		bool isIntersects(const Bbox& other) const {
			return !(other.maxX < minX || other.minX > maxX ||
				other.maxY < minY || other.minY > maxY);
		}
		// 检查当前包围盒是否被另一个包围盒完全包含
		bool isBeContained(const Bbox& other) const
		{
			return minX >= other.minX && maxX <= other.maxX &&
				minY >= other.minY && maxY <= other.maxY;
		}

		// 返回两个包围盒的交集区域，需要保证相交才能调用。
		Bbox getInterBox(const Bbox& p)
		{
			return { std::max(minX, p.minX), std::max(minY, p.minY), std::min(maxX, p.maxX), std::min(maxY, p.maxY) };
		}

		int getMinX() const
		{
			return minX;
		}
		int getMinY() const
		{
			return minY;
		}
		int getMaxX() const
		{
			return maxX;
		}
		int getMaxY() const
		{
			return maxY;
		}
		int getWidth() const
		{
			return getMaxX() - getMinX();
		}
		int getHeight() const
		{
			return getMaxY() - getMinY();
		}
	};
} // namespace MBSO