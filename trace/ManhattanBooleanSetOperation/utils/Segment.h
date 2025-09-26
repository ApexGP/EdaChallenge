#pragma once
#include "../const/Const.h"
#include "Point_2.h"
#include "Bbox.h"

class Segment_2
{
public:
	TYPE type;          // 类型， SEG or ARC
	Point_2 start;      // 起点
	Point_2 end;        // 终点
	Point_2 center;		// 圆心
	Point_2 vSeg;		// 线段向量
	double radius;		// 半径
	double angle;		// 圆心角
	double length;      // 长度
	AABBbox box;		// 包围盒

public:
	Segment_2() : radius(0), angle(0), length(0) {}

	// 线段构造
	Segment_2(Point_2 _start, Point_2 _end) :
		start(_start), end(_end), vSeg(end - start), type(SEG), radius(0), angle(0)
	{
		length = hypot(start.getX() - end.getX(), start.getY() - end.getY());
		box.minX = min(start.getX(), end.getX());
		box.maxX = max(start.getX(), end.getX());
		box.minY = min(start.getY(), end.getY());
		box.maxY = max(start.getY(), end.getY());
	}
	// 圆弧构造
	Segment_2(Point_2 _start, Point_2 _end, Point_2 _center, double _radius, double _angle) :
		start(_start), end(_end), vSeg(end - start), center(_center), radius(_radius), angle(_angle), type(ARC)
	{
		length = fabs(radius * angle);
		Point_2 up(center.getX(), center.getY() + radius);
		Point_2 down(center.getX(), center.getY() - radius);
		Point_2 left(center.getX() - radius, center.getY());
		Point_2 right(center.getX() + radius, center.getY());
		box.update(start);
		box.update(end);
		if (pointOnArc(up)) box.update(up);
		if (pointOnArc(down)) box.update(down);
		if (pointOnArc(left)) box.update(left);
		if (pointOnArc(right)) box.update(right);
	}
	// 圆弧构造
	Segment_2(Point_2 start_p, Point_2 end_p, double angle) :
		start(start_p), end(end_p), vSeg(end - start), angle(angle), type(ARC)
	{
		arc_get_center_radius();
		length = fabs(radius * angle);
		Point_2 up(center.getX(), center.getY() + radius);
		Point_2 down(center.getX(), center.getY() - radius);
		Point_2 left(center.getX() - radius, center.getY());
		Point_2 right(center.getX() + radius, center.getY());
		box.update(start);
		box.update(end);
		if (pointOnArc(up)) box.update(up);
		if (pointOnArc(down)) box.update(down);
		if (pointOnArc(left)) box.update(left);
		if (pointOnArc(right)) box.update(right);
	}
	// 圆构造，转成圆弧
	Segment_2(Point_2 _center, double _radius, double _angle) :
		start(_center.getX() + _radius, _center.getY()),
		end(start), center(_center),
		radius(_radius), angle(_angle), type(ARC)
	{
		length = 2 * pi * radius;
		box.minX = center.getX() - radius;
		box.maxX = center.getX() + radius;
		box.minY = center.getY() - radius;
		box.maxY = center.getY() + radius;
	}

	Point_2& source()
	{
		return start;
	}
	Point_2& target()
	{
		return end;
	}

	void arc_get_center_radius()
	{
		double angle_tmp = angle;
		angle_tmp = fabs(angle_tmp);
		double se_len = hypot(start.getX() - end.getX(), start.getY() - end.getY());
		if (sgn(angle_tmp - pi, eps_double) == 0) radius = se_len / 2.0;
		else if (sgn(angle_tmp - pi, eps_double) > 0) angle_tmp = 2 * pi - angle_tmp; // 大于 pi
		// 小于 pi
		radius = se_len / 2 / sin(angle_tmp / 2);

		double arc_len = fabs(angle) * radius;
		// end_p 绕着 start_p 旋转
		double rotate_angle = (pi - angle) / 2;
		if (sgn(angle, eps_double) < 0) rotate_angle -= pi;
		Point_2 v = end - start;
		double c = cos(rotate_angle), s = sin(rotate_angle);
		Point_2 rotate_e = Point_2(start.getX() + v.getX() * c - v.getY() * s,
			start.getY() + v.getX() * s + v.getY() * c);
		v = rotate_e - start;
		center = start + v / se_len * radius;
	}

	void setSeg(Point_2& _start, Point_2& _end)
	{
		start = _start;
		end = _end;
		vSeg = end - start;
		type = SEG;
		length = hypot(start.x - end.x, start.y - end.y);
		box.minX = min(start.x, end.x);
		box.maxX = max(start.x, end.x);
		box.minY = min(start.y, end.y);
		box.maxY = max(start.y, end.y);
	}

	void setArc(Point_2& _start, Point_2& _end, Point_2& _center, double _radius, double _angle)
	{
		start = _start;
		end = _end;
		center = _center;
		radius = _radius;
		angle = _angle;
		vSeg = end - start;
		length = fabs(radius * angle);
		type = ARC;
		box.init();
		box.update(start);
		box.update(end);
		Point_2 up(center.getX(), center.getY() + radius);
		Point_2 down(center.getX(), center.getY() - radius);
		Point_2 left(center.getX() - radius, center.getY());
		Point_2 right(center.getX() + radius, center.getY());
		if (pointOnArc(up)) box.update(up);
		if (pointOnArc(down)) box.update(down);
		if (pointOnArc(left)) box.update(left);
		if (pointOnArc(right)) box.update(right);
	}

	double getLength() const
	{
		return length;
	}

	void reverse()
	{
		swap(start, end);
		vSeg *= -1;
		angle *= -1;
	}

	// 求该线段过 p 点的切线向量
	Point_2 tangentVector(Point_2& p)
	{
		// 轮廓顺，p->center 向量逆旋转，轮廓逆，p->center 向量顺旋转
		if (type == SEG) return end - p;
		else return (center - p).rotateVector90(-sgn(angle, eps_double));
	}

	// 求该线段过 p 点的切线向量上的点, 也就是顺着圆弧方向的切线终点。
	Point_2 tangentPointAfter(Point_2& p)
	{
		if (type == SEG) return end;
		//else return center.rotatePoint90(p, -sgn(angle, eps_double));
		else return center.rotatePoint(p, -sgn(angle, eps_double) * pi * 4 / 9);
	}
	// 求该线段过 p 点的切线向量上的点, p为终点，求切线向量起点。也就是逆着圆弧方向的切线起点。
	Point_2 tangentPointBefore(Point_2& p)
	{
		if (type == SEG) return start;
		//else return center.rotatePoint90(p, sgn(angle, eps_double));
		else return center.rotatePoint(p, sgn(angle, eps_double) * pi * 4 / 9);

	}

	double distanceFromOri(const Point_2& p)
	{
		double delta_x = p.getX() - start.getX();
		double delta_y = p.getY() - start.getY();
		return hypot(delta_x, delta_y);
	}
	// 判断点是否在线段上
	bool pointOnSeg(Point_2& p)
	{
		Point_2 vOriP = (p - start).trunc(1);
		Point_2 vOriDest = vSeg.trunc(1);
		Point_2 vDestP = (p - end).trunc(1);
		// 共线，反向
		return sgn(vOriP ^ vOriDest, eps_double) == 0 && sgn(vOriP * vDestP, eps_double) <= 0;
	}
	// 判断点是否在圆弧上
	bool pointOnArc(Point_2& p)
	{
		if (type != ARC) return false;
		if (sgn(center.distance(p) - radius, eps_double) != 0) return false;
		if (sgn(angle, eps_double) >= 0) {
			return sgn(center.radCCW(start, p) - angle, eps_double) <= 0;
		}
		else {
			double alpha = center.radCCW(start, p);
			return sgn(alpha, eps_double) == 0 || sgn(alpha - 2 * pi - angle, eps_double) >= 0;
		}
	}
	// 点a,b是否在直线 this 的两侧
	bool twoSide(Point_2& a, Point_2& b)
	{
		Point_2 vOriA = (a - start).trunc(1);
		Point_2 vvSeg = vSeg.trunc(1);
		Point_2 vOriB = (b - start).trunc(1);
		// 或者 二者异或为 -2，-1^1，1^-1
		return sgn(vOriA ^ vvSeg, eps_double) * sgn(vOriB ^ vvSeg, eps_double) < 0;
	}

	// 两线段是否相交
	// return 不相交 NOT_INTER
	//		  相交
	//		  平行，且重合出一个交点或者线段
	bool isSegCrossSeg(Segment_2& v)
	{
		// 快速排斥实验
		if (!box.isCollisions(v.box)) return false;
		//if (pointOnSeg(v.start) || pointOnSeg(v.end)) return true;
		//if (v.pointOnSeg(start) || v.pointOnSeg(end)) return true;
		//// 跨立实验
		//return twoSide(v.start, v.end) && v.twoSide(start, end);
		if (parallel(v)) {
			// 如果平行了，就需要判断点是否在线段上
			if (pointOnSeg(v.start) || pointOnSeg(v.end)) return true;
			if (v.pointOnSeg(start) || v.pointOnSeg(end)) return true;
		}
		else {
			Point_2 p = lineInterPoint(v);
			if (pointOnSeg(p) && v.pointOnSeg(p)) return true;
		}
		return false;
	}

	POINT_TYPE isSegCrossSegAndInter(Segment_2& v, Point_2& p, Point_2& q)
	{
		if (!box.isCollisions(v.box)) return NOT_INTER;
		if (parallel(v)) {
			// 如果平行了，就需要判断点是否在线段上
			bool ret = false;
			if (pointOnSeg(v.start) || pointOnSeg(v.end)) ret = true;
			if (v.pointOnSeg(start) || v.pointOnSeg(end)) ret = true;
			if (!ret) return NOT_INTER;
			//return NOT_INTER;
			vector<Point_2> a = { start, end, lineProject(v.start), lineProject(v.end) };
			sort(a.begin(), a.end(), [](const Point_2& x, const Point_2& y) {
				return sgn(x.getX() - y.getX(), eps_double) == 0 ? sgn(x.getY() - y.getY(), eps_double) < 0 : sgn(x.getX() - y.getX(), eps_double) < 0;
				//return x.p.getX() == y.p.getX() ? x.p.getY() < y.p.getY() : x.p.getX() < y.p.getX();
				});
			if (a[1].equals(a[2], eps_double))
			{
				p = a[1];
				return INTER_POINT;
			}
			p = a[1], q = a[2];
			return INTER_SEG;
		}
		else {
			p = lineInterPoint(v);
			if (pointOnSeg(p) && v.pointOnSeg(p)) {
				return INTER_POINT;
			}
		}
		return NOT_INTER;
	}

	// this 线段，v Arc
	// 直线和圆交点，判断交点是否在线段和圆弧上。
	POINT_TYPE isSegCrossArcAndInter(Segment_2& v, Point_2& p, Point_2& q)
	{
		if (!box.isCollisions(v.box)) return NOT_INTER; // 包围盒是否相交
		// 线段和圆的关系，比较圆心到线段的距离和半径的关系
		int ret = sgn(disPointFromSeg(v.center) - v.radius, eps_double);
		if (ret > 0) return NOT_INTER;
		// 线段和圆有交点，求出交点，判断交点是否在线段和圆弧上。
		Point_2 a = lineProject(v.center); // 圆心到直线的投影。
		double dis = disPointFromLine(v.center); // 圆心到直线的距离
		double d2 = v.radius * v.radius - dis * dis;
		// 旋转得到两个点
		if (sgn(d2, eps_double) == 0) {
			// 最多只有一个交点
			p = a;
			if (pointOnSeg(p) && v.pointOnArc(p)) return INTER_POINT;
			else return NOT_INTER;
		}
		else {
			// 可能有两个交点
			double d = sqrt(d2);
			p = a + vSeg.trunc(d);
			if (pointOnSeg(p) && v.pointOnArc(p)) {
				q = a - vSeg.trunc(d);
				if (pointOnSeg(q) && v.pointOnArc(q)) return INTER_SEG;
				else return INTER_POINT;
			}
			else {
				p = a - vSeg.trunc(d);
				if (pointOnSeg(p) && v.pointOnArc(p)) return INTER_POINT;
				else return NOT_INTER;
			}
		}
	}

	// this Arc, v Arc, 圆弧与圆弧判断是否相交并求交点
	POINT_TYPE isArcCrossArcAndInter(Segment_2& v, Point_2& p, Point_2& q)
	{
		if (!box.isCollisions(v.box)) return NOT_INTER;
		double centerDis = center.distance(v.center);
		// 圆心之间的距离大于半径和，或者小于半径差。
		if (sgn(centerDis - radius - v.radius, eps_double) > 0 ||
			sgn(centerDis - abs(radius - v.radius), eps_double) < 0) return NOT_INTER;
		// 圆心重合
		if (sgn(centerDis, eps_double) == 0) {
			if (sgn(radius - v.radius, eps_double) != 0) return NOT_INTER;
			// 完全重合
			bool ret = false;
			if (pointOnArc(v.start) || pointOnArc(v.end)) ret = true;
			if (v.pointOnArc(start) || v.pointOnArc(end)) ret = true;
			if (!ret) return NOT_INTER;
			// 好像可以不用管。
			return NOT_INTER;
			//vector<Point_2> a = {start, end, }
		}
		// 两个圆有交点，求出两个交点
		double l = (centerDis * centerDis + radius * radius - v.radius * v.radius) / (2 * centerDis);
		double h2 = radius * radius - l * l;
		// 只有一个交点
		if (sgn(h2, eps_double) == 0) h2 = 0;
		double h = sqrt(h2);
		Point_2 vCenter = v.center - center;
		Point_2 tmp = center + vCenter.trunc(l);
		p = tmp + (vCenter.rotateVector90(1).trunc(h)); // 逆时针旋转90
		if (pointOnArc(p) && v.pointOnArc(p)) {
			q = tmp + (vCenter.rotateVector90(-1).trunc(h)); // 顺时针旋转90
			if (pointOnArc(q) && v.pointOnArc(q)) return INTER_SEG;
			else return INTER_POINT;
		}
		else {
			p = tmp + (vCenter.rotateVector90(-1).trunc(h)); // 顺时针旋转90
			if (pointOnArc(p) && v.pointOnArc(p)) return INTER_POINT;
			else return NOT_INTER;
		}
	}

	// 传入p，求p到 直线 this的距离
	double disPointFromLine(Point_2& p)
	{
		return fabs((p - start) ^ vSeg) / getLength();
	}

	// 传入p，求p到 线段 this的距离
	double disPointFromSeg(Point_2& p)
	{
		if (sgn((p - start) * vSeg, eps_double) < 0 || sgn((end - p) * vSeg, eps_double) < 0)
			return min(start.distance(p), end.distance(p));
		else return disPointFromLine(p);
	}


	Point_2 lineProject(Point_2& p)
	{
		Point_2 vSeg = end - start;
		Point_2 oriP = p - start;
		return start + ((vSeg * (vSeg * oriP)) / vSeg.length2());
	}

	// 线段和线段的交点，需要先调用一下isSegCrossSeg保证相交
	// 返回值 INTER_SEG 表示返回的是线段，取 seg
	// 返回值 INTER_POINT 表示返回的是点，取 p
	POINT_TYPE segInterPoint(Segment_2& v, Point_2& p, Point_2& q)
	{
		if (!parallel(v))
		{
			p = lineInterPoint(v);
			return INTER_POINT;
		}
		// 平行，并且相交。
		vector<Point_2> a = { start, end, lineProject(v.start), lineProject(v.end) };
		sort(a.begin(), a.end(), [](const Point_2& x, const Point_2& y) {
			return sgn(x.getX() - y.getX(), eps_double) == 0 ? sgn(x.getY() - y.getY(), eps_double) < 0 : sgn(x.getX() - y.getX(), eps_double) < 0;
			//return x.p.getX() == y.p.getX() ? x.p.getY() < y.p.getY() : x.p.getX() < y.p.getX();
			});
		if (a[1].equals(a[2], eps_double))
		{
			p = a[1];
			return INTER_POINT;
		}
		p = a[1], q = a[2];
		return INTER_SEG;
	}

	// 直线与直线交点
	// 需要保证直线不平行，不重合
	Point_2 lineInterPoint(const Segment_2& v)
	{
		Point_2 vv = v.end - v.start;
		double a1 = vv ^ (start - v.start);
		double a2 = vv ^ (end - v.start);
		return Point_2((start.getX() * a2 - end.getX() * a1) / (a2 - a1),
			(start.getY() * a2 - end.getY() * a1) / (a2 - a1));
	}

	bool parallel(const Segment_2& v)
	{
		Point_2 v1 = vSeg.trunc(1);
		Point_2 v2 = v.vSeg.trunc(1);
		return sgn(v1 ^ v2, eps_double) == 0;
	}

	bool sameDir(const Segment_2& v)
	{
		Point_2 v1 = vSeg.trunc(1);
		Point_2 v2 = v.vSeg.trunc(1);
		return sgn(v1 ^ v2, eps_double) == 0 && sgn(v1 * v2, eps_double) > 0;
	}

	double getCrossArea() {
		return type == SEG ? (start ^ end) : ((start ^ center) + (center ^ end) + radius * radius * angle);
	}

	// 圆弧，通过 x 坐标得到 y 坐标。返回值为y坐标个数，不存在0，一个y1, 两个 y1,y2
	int arcGetYByX(double x, double& y1, double& y2)
	{
		// 圆的方程 r^2 = (x - center.getX()) ^ 2 + (y - center.getY())^2
		double yy = radius * radius - (x - center.getX()) * (x - center.getX());
		int ret = sgn(yy, eps_double);
		if (ret >= 0) {
			if (ret == 0) {
				y1 = center.getY();
				y2 = 0;
				return 1;
			}
			else {
				yy = sqrt(yy);
				y1 = yy + center.getY();
				y2 = -yy + center.getY();
				return 2;
			}
		}
		else return 0;
	}

	int arcGetXByY(double y, double& x1, double& x2)
	{
		double xx = radius * radius - (y - center.getY()) * (y - center.getY());
		int ret = sgn(xx, eps_double);
		if (ret >= 0) {
			if (ret == 0) {
				x1 = center.getX();
				x2 = 0;
				return 1;
			}
			else {
				xx = sqrt(xx);
				x1 = xx + center.getX();
				x2 = -xx + center.getX();
				return 2;
			}
		}
		else return 0;
	}
};

#endif