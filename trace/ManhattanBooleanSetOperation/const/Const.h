#pragma once
#include <numeric>
namespace MBSO
{
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

	// 多边形方向
	enum DIR
	{
		CW,		// 顺时针，轮廓
		CCW,	// 逆时针，洞
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