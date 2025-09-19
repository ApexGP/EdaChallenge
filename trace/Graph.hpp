#pragma once

#include "public.h"

class Graph {
public:
    Graph(){}
    // 初始化节点数量
    explicit Graph(int num_nodes): _node_count(num_nodes), _offsets(num_nodes + 1, 0) {}

    // 批量添加边
    void AddEdges(const std::vector<Edge>& edges) {
        // 第一次遍历：统计节点度数
        std::vector<int> degrees(_node_count, 0);
        for (const auto& edge : edges) {
            int u = edge.first;
            int v = edge.second;
            if (u < 0 || u >= _node_count || v < 0 || v >= _node_count) continue;
            ++degrees[u];
            ++degrees[v];
        }

        // 计算CSR偏移量
        for (size_t i = 1; i <= _node_count; ++i) {
            _offsets[i] = _offsets[i - 1] + degrees[i - 1];
        }

        // 分配邻接表内存
        _edges.resize(_offsets[_node_count]);
        std::vector<size_t> curr_index(_offsets.begin(), _offsets.begin() + _node_count);

        // 第二次遍历：填充邻接表
        for (const auto& edge : edges) {
            int u = edge.first;
            int v = edge.second;
            if (u < 0 || u >= _node_count || v < 0 || v >= _node_count) continue;
            _edges[curr_index[u]++] = v;
            _edges[curr_index[v]++] = u;
        }
    }

    // 获取连通分量（BFS实现）
    std::vector<int> GetConnectedComponent(int start) const {
        if (start < 0 || start >= _node_count) return {};

        // 位图记录访问状态（1bit/node）
        std::vector<bool> visited(_node_count, false);
        std::vector<int> component;
        component.reserve(_node_count / 10); // 预分配内存

        // 手动管理队列避免deque开销
        std::vector<int> queue(_node_count);
        size_t front = 0, rear = 0;

        queue[rear++] = start;
        visited[start] = true;
        component.emplace_back(start);

        while (front < rear) {
            int current = queue[front++];
            // 遍历当前节点的邻居
            size_t start_idx = _offsets[current];
            size_t end_idx = _offsets[current + 1];

            for (size_t i = start_idx; i < end_idx; ++i) {
                int neighbor = _edges[i];
                if (!visited[neighbor]) {
                    visited[neighbor] = true;
                    queue[rear++] = neighbor;
                    component.emplace_back(neighbor);
                }
            }
        }
        return component;
    }

private:
    int _node_count;                 // 节点总数
    std::vector<int> _offsets;       // CSR偏移数组
    std::vector<int> _edges;         // 领接表边存储
};