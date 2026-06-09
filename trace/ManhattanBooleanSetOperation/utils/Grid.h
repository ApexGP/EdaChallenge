#pragma once
#include <vector>

namespace MBSO {
	using std::vector;
	using std::pair;

	// 网格类，存储二维网格，每个网格存储一个vector
	template<class T>
	class Grid
	{
	public:
		vector<vector<vector<T>>> grid; // 二维网格, 每个grid存储一个vector
		vector<pair<int, int>> usedId;  // 使用过的grid id
		int sizeX, sizeY;

		Grid(int _sizeX, int _sizeY) :
			grid(_sizeX, vector<vector<T>>(_sizeY)),
			sizeX(_sizeX), sizeY(_sizeY)
		{
			usedId.reserve(sizeX * sizeY);
			for (int i = 0; i < sizeX; ++i)
				for (int j = 0; j < sizeY; ++j) grid[i][j].reserve(100);
		}
		Grid() : sizeX(0), sizeY(0) {}

		// 调整grid大小
		void resize(int _sizeX, int _sizeY)
		{
			sizeX = _sizeX;
			sizeY = _sizeY;
			grid.resize(sizeX, vector<vector<T>>(sizeY));
			for (int i = 0; i < sizeX; ++i)
				for (int j = 0; j < sizeY; ++j) grid[i][j].reserve(100);
			usedId.clear();
			usedId.reserve(sizeX * sizeY);
		}

		// 重置grid大小
		void reset(int _sizeX, int _sizeY)
		{
			resize(_sizeX, _sizeY);
		}

		// 清空所有使用过的grid
		void clear()
		{
			int n = usedId.size();
			for (int i = 0; i < n; ++i)
			{
				if (grid[usedId[i].first][usedId[i].second].size() == 0) continue;
				grid[usedId[i].first][usedId[i].second].clear();
			}
			usedId.clear();
		}

		// 在grid[x][y]中添加元素element
		void emplace_back(int x, int y, T& element)
		{
			grid[x][y].emplace_back(element);
			usedId.emplace_back(x, y);
		}

		// 获取grid[x][y]中的vector
		vector<T>& getGridID(int x, int y)
		{
			return grid[x][y];
		}
	};

} // namespace MBSO