#pragma once
#include "../const/Const.h"

namespace MBSO
{
	// 二维点或向量
    class MPoint_2
    {
    public:
        int  x, y;
        MPoint_2() : x(0), y(0) {}

        MPoint_2(int _x, int _y) : x(_x), y(_y) {}

        // 重载比较符
        bool operator==(const MPoint_2& p) const { return p.x == x && p.y == y; }
        bool operator<(const MPoint_2& p) const { return p.x == x ? y < p.y : x < p.x; }
        bool operator>(const MPoint_2& p) const { return p < (*this); }

        // 重载负号
        MPoint_2 operator-() const { return MPoint_2(-x, -y); }

		// 重载加减
        MPoint_2 operator+(const MPoint_2& p) const { return MPoint_2(x + p.x, y + p.y); }
        MPoint_2 operator-(const MPoint_2& p) const { return MPoint_2(x - p.x, y - p.y); }

        // 叉积
        double operator^(const MPoint_2& p) const
        {
            return static_cast<double>(x) * static_cast<double>(p.y) - static_cast<double>(y) * static_cast<double>(p.x);
        }
        // 点积
        double operator*(const MPoint_2& p) const
        {
            return static_cast<double>(x) * static_cast<double>(p.x) + static_cast<double>(y) * static_cast<double>(p.y);
        }

		// 获取向量长度
        double length() const
        {
            return hypot(x, y);
        }
		// 获取x坐标
        int getX() const
        {
            return x;
        }
		// 获取y坐标
        int getY() const
        {
            return y;
        }

        // 角的夹角，角顶点为 this, 两个端点为 p1, p2;
        double rad(const MPoint_2& p1, const MPoint_2& p2)
        {
            MPoint_2 v1(p1.x - x, p1.y - y);
            MPoint_2 v2(p2.x - x, p2.y - y);
            return radVector(v1, v2);
        }
        // 向量夹角 [0, pi], 传进来两个向量
        double radVector(const MPoint_2& v1, const MPoint_2& v2)
        {
            double cross = v1 ^ v2;
            double dot = v1 * v2;
            double angle = fabs(atan2(fabs(cross), dot));
            return angle;
        }
    };

} // namespace MBSO