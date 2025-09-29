#pragma once
#include "../const/Const.h"
#include "MPoint_2.h"

namespace MBSO {
	// 曼哈顿线段类(曼哈顿多边形只有水平线或垂直线)
	class MSegment_2
	{
	public:
		MPoint_2 start;     // 起点
		MPoint_2 end;       // 终点
		MPoint_2 vSeg;		// 线段向量
		bool is_horizontal; // 是否水平
		int length;			// 长度

	public:

		MSegment_2() : is_horizontal(false), length(0) {}
		// 线段构造
		MSegment_2(MPoint_2 _start, MPoint_2 _end) :
			start(_start), end(_end)
		{
			vSeg = end - start;
			is_horizontal = (_start.getY() == _end.getY());
			length = is_horizontal ? abs(vSeg.getX()) : abs(vSeg.getY());
		}

		// 返回线段的起点
		const MPoint_2& source() const
		{
			return start;
		}
		// 返回线段的终点
		const MPoint_2& target() const
		{
			return end;
		}
		// 使用两点设置线段
		void setSeg(MPoint_2& _start, MPoint_2& _end)
		{
			start = _start;
			end = _end;
			vSeg = end - start;
			is_horizontal = (_start.getY() == _end.getY());
			length = is_horizontal ? abs(vSeg.getX()) : abs(vSeg.getY());
		}
		// 获取线段长度
		int getLength() const
		{
			return length;
		}

		// 反转线段方向
		void reverse()
		{
			std::swap(start, end);
			vSeg = -vSeg;
		}

		// 计算点到线段起点的距离
		double distanceFromOri(const MPoint_2& p) const
		{
			double delta_x = p.getX() - start.getX();
			double delta_y = p.getY() - start.getY();
			return hypot(delta_x, delta_y);
		}

		/*
		// 判断点是否在曼哈顿线段上
		bool isPointOnSeg(MPoint_2& p) const
		{
			if (is_horizontal) { // 水平线
				int x1 = start.getX(), x2 = end.getX();
				if (x1 > x2) std::swap(x1, x2);
				return p.getY() == start.getY() && x1 <= p.getX() && p.getX() <= x2;
			}
			else { // 垂直线
				int y1 = start.getY(), y2 = end.getY();
				if (y1 > y2) std::swap(y1, y2);
				return p.getX() == start.getX() && y1 <= p.getY() && p.getY() <= y2;
			}
		}
		
		// 两曼哈顿线段是否相交
		bool isSegCrossSeg(MSegment_2& v)
		{
			// 情况1：两条都是水平边
			if (is_horizontal && v.is_horizontal) {
				if (start.getY() == v.start.getY()) { // 同一水平线
					// 检测线段重叠
					int x1 = start.getX(), x2 = end.getX();
					if (x1 > x2) std::swap(x1, x2);
					int x3 = v.start.getX(), x4 = v.end.getX();
					if (x3 > x4) std::swap(x3, x4);
					return !(x2 < x3 || x4 < x1);
				}
				return false;
			}

			// 情况2：两条都是垂直边
			if (!is_horizontal && !v.is_horizontal) {
				if (start.getX() == v.start.getX()) { // 同一垂直线
					// 检测线段重叠
					int y1 = start.getY(), y2 = end.getY();
					if (y1 > y2) std::swap(y1, y2);
					int y3 = v.start.getY(), y4 = v.end.getY();
					if (y3 > y4) std::swap(y3, y4);
					return !(y2 < y3 || y4 < y1);
				}
				return false;
			}

			// 情况3：一条水平边，一条垂直边
			const MSegment_2* h_edge, * v_edge;
			if (is_horizontal) h_edge = this, v_edge = &v;
			else h_edge = &v, v_edge = this;

			// 检测水平边和垂直边的交点
			int x1 = h_edge->start.getX(), x2 = h_edge->end.getX();
			if (x1 > x2) std::swap(x1, x2);
			int y1 = v_edge->start.getY(), y2 = v_edge->end.getY();
			if (y1 > y2) std::swap(y1, y2);

			// 相互跨立才能相交
			return (x1 <= v_edge->start.getX() && v_edge->start.getX() <= x2 &&
				y1 <= h_edge->start.getY() && h_edge->start.getY() <= y2);
		}
		*/

		// 判断两曼哈顿线段是否相交，若相交还需计算并返回交点
		// 返回值 NOT_INTER 表示无交点，引用参数 p, q 无意义
		// 返回值 INTER_SEG 表示相交部分是线段，取 p, q两交点
		// 返回值 INTER_POINT 表示相交部分是点，取 p 交点，q 无意义
		POINT_TYPE isSegCrossSegAndInter(MSegment_2& v, MPoint_2& p, MPoint_2& q)
		{
			// 情况1：两条都是水平边
			if (is_horizontal && v.is_horizontal) {
				if (start.getY() == v.start.getY()) { // 同一水平线
					// 检测线段重叠
					int x1 = start.getX(), x2 = end.getX();
					if (x1 > x2) std::swap(x1, x2);
					int x3 = v.start.getX(), x4 = v.end.getX();
					if (x3 > x4) std::swap(x3, x4);

					// 线段不重叠
					if (x2 < x3 || x4 < x1) return NOT_INTER;
					else { // 有重叠
						int xx1 = std::max(x1, x3), xx2 = std::min(x2, x4);
						if (xx1 == xx2) { // 交点是一个点
							p = MPoint_2(xx1, start.getY());
							return INTER_POINT;
						}
						else { // 交点是一条线段
							p = MPoint_2(xx1, start.getY());
							q = MPoint_2(xx2, start.getY());
							return INTER_SEG;
						}
					}
				}
				return NOT_INTER;
			}

			// 情况2：两条都是垂直边
			if (!is_horizontal && !v.is_horizontal) {
				if (start.getX() == v.start.getX()) { // 同一垂直线
					// 检测线段重叠
					int y1 = start.getY(), y2 = end.getY();
					if (y1 > y2) std::swap(y1, y2);
					int y3 = v.start.getY(), y4 = v.end.getY();
					if (y3 > y4) std::swap(y3, y4);

					// 线段不重叠
					if (y2 < y3 || y4 < y1) return NOT_INTER;
					else { // 有重叠
						int yy1 = std::max(y1, y3), yy2 = std::min(y2, y4);
						if (yy1 == yy2) { // 交点是一个点
							p = MPoint_2(start.getX(), yy1);
							return INTER_POINT;
						}
						else { // 交点是一条线段
							p = MPoint_2(start.getX(), yy1);
							q = MPoint_2(start.getX(), yy2);
							return INTER_SEG;
						}
					}
				}
				return NOT_INTER;
			}

			// 情况3：一条水平边，一条垂直边
			const MSegment_2* h_edge, * v_edge;
			if (is_horizontal) h_edge = this, v_edge = &v;
			else h_edge = &v, v_edge = this;

			// 检测水平边和垂直边的交点
			int x1 = h_edge->start.getX(), x2 = h_edge->end.getX();
			if (x1 > x2) std::swap(x1, x2);
			int y1 = v_edge->start.getY(), y2 = v_edge->end.getY();
			if (y1 > y2) std::swap(y1, y2);

			// 若相互跨立则相交于一个点
			if (x1 <= v_edge->start.getX() && v_edge->start.getX() <= x2 &&
				y1 <= h_edge->start.getY() && h_edge->start.getY() <= y2) {
				p = MPoint_2(v_edge->start.getX(), h_edge->start.getY());
				return INTER_POINT;
			}
			else return NOT_INTER;
		}
	};

} // namespace MBSO