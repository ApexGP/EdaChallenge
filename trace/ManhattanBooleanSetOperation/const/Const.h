#pragma once
#include <numeric>
#include <utility>
#include <functional>
#include <algorithm>
#include <cmath>
#include <climits>
namespace MBSO
{

	static constexpr double eps_double = 1e-7;
	static constexpr int INF = 0x3f3f3f3f;
	// 符号函数
	inline int sgn(const double& x, const double& eps)
	{
		if (fabs(x) < eps)
			return 0;
		return x < 0 ? -1 : 1;
	}

	using PII = std::pair<int, int>;

	struct pair_hash
	{
		template<class T1, class T2>
		std::size_t operator() (const std::pair<T1, T2>& p) const
		{
			auto h1 = std::hash<T1>{}(p.first);
			auto h2 = std::hash<T2>{}(p.second);
			return h1 ^ h2;
		}
	};

	// 布尔运算类型
	enum OP_TYPE
	{
		INTER,	// 求交集
		UNION,	// 求并集
		DIFF,   // 求差集
	};

	// 多边形方向，外轮廓的点顺序为顺时针，孔洞的点顺序为逆时针
	enum DIR
	{
		CW,		// 顺时针，外轮廓
		CCW,	// 逆时针，孔洞
	};

	// 相交类型
	enum POINT_TYPE {
		POINT_POINT,	// 端点和端点，非规范相交
		POINT_SEG,		// 端点和线段，非规范相交
		SEG_SEG,		// 线段和线段，规范相交
		NOT_INTER,		// 不相交
		INTER_SEG,		// 相交成线段了
		INTER_POINT,	// 相交成了点
		POINT_2,
	};

	// 交点类型
	enum INTER_TYPE
	{
		IN_POINT = 0,		// 入点
		OUT_POINT = 1,		// 出点
		UNKNOWN = 2,		// 未知
	};

	static const char* ins_list[]{
		"",
		"",
		""
	};
} // namespace MBSO