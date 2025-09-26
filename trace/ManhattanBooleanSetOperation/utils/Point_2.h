#pragma once
#include "../const/Const.h"

namespace MBSO
{
	// 二维点或向量
    class Point_2
    {
    public:
        int  x, y;
        Point_2() : x(0), y(0) {}

        Point_2(int _x, int _y) : x(_x), y(_y) {}

        // 重载比较符
        bool operator==(const Point_2& p) const { return p.x == x && p.y == y; }
        bool operator<(const Point_2& p) const { return p.x == x ? y < p.y : x < p.x; }
        bool operator>(const Point_2& p) const { return p < (*this); }

		// 重载加减
        Point_2 operator+(const Point_2& p) const { return Point_2(x + p.x, y + p.y); }
		Point_2 operator-(const Point_2& p) const { return Point_2(x - p.x, y - p.y); }

        // 叉积
        long long operator^(const Point_2& p) const
        {
            return static_cast<long long>(x) * static_cast<long long>(p.y) - static_cast<long long>(y) * static_cast<long long>(p.x);
        }
        // 点积
        long long operator*(const Point_2& p) const
        {
            return static_cast<long long>(x) * static_cast<long long>(p.x) + static_cast<long long>(y) * static_cast<long long>(p.y);
        }
     
        int getX() const
        {
            return x;
        }
        int getY() const
        {
            return y;
        }
    };

} // namespace MBSO