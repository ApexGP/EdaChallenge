#pragma once
#include "public.h"
#include "Input.hpp"
#include "PolygonCutting.hpp"
#include "SpaceIndex.hpp"
#include "Intersect.hpp"
#include "Graph.hpp"
#include <queue>
#include <vector>
#include <limits>
#include <algorithm>

/* 链路追踪封装类声明 */
class Trace {
private:
	Input& input;

public:
    Trace(Input& _input): input(_input){}

	/* 追踪方法-单线程 */
	std::vector<int> TraceUsingCompleteGraph(); // 先完全建图再BFS
	std::vector<int> TraceUsingLazyGraph();		// BFS驱动延迟建图

	/* 追踪方法-多线程 */
	std::vector<int> TraceUsingCompleteGraphParallel(int thread_count); // 先完全建图再BFS
	std::vector<int> TraceUsingLazyGraphParallel(int thread_count);		// BFS驱动延迟建图

private:
	// Lazy BFS：仅按需扩展起点可达区域
	std::vector<int> RunLazyConnectedComponent(SpaceIndex& spaceIndex, Intersect& intersect, int start_id,
		std::vector<bool>& bfs_visted, const robin_hood::unordered_map<int, std::vector<int>>* extra_adj = nullptr);
	// 并行版本 Lazy BFS：仅按需扩展起点可达区域
	std::vector<int> RunLazyConnectedComponentParallel(SpaceIndex &spaceIndex, Intersect &intersect, int start_id,
		std::vector<bool> &bfs_visted, const robin_hood::unordered_map<int, std::vector<int>> *extra_adj, int thread_count);
};

#pragma region implement

// 先完全建图再BFS-单线程
std::vector<int> Trace::TraceUsingCompleteGraph() {
	Timer myTimer;
	if (!input.has_gate_rule) // 若没有Gate规则(单起点)
	{
		/* 根据输入和规则建立空间索引 */
		std::cout << "----- Starting Space Index -----" << std::endl;
		SpaceIndex spaceIndex(input); 
		spaceIndex.CreatSpaceIndexMerged();	// Via联通的两层合并建树
		spaceIndex.PrintSpaceIndexInfo();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 获取起点所在多边形id */
		std::cout << "----- Starting Get StartPos -----" << std::endl;
		int start_pos_id = spaceIndex.GetStartPosinPolygonId(input.start_pos[0]);
		std::cout << "StartPos Polygon id:" << start_pos_id << std::endl;
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 相交检测获取边集 */
		std::cout << "----- Starting Intersection Test -----" << std::endl;
		Intersect intersect(input, spaceIndex);
		std::vector<Edge> edges = intersect.getAllEdge();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 建图并求连通分量 */
		std::cout << "----- Starting Get Connected Component -----" << std::endl;
		Graph graph(input.total_polygon);
		graph.AddEdges(edges);
		std::vector<int> component = graph.GetConnectedComponent(start_pos_id);
		std::cout << "s1 connected polygon size: " << component.size() << std::endl;
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		return component;
	}
	else // 有Gate规则(双起点)
	{
		/* PO层多边形合并与AA层多边形切割 */
		std::cout << "----- Starting Merge and Cutting Polygon -----" << std::endl;
		PolygonCutting cutting(input);
		robin_hood::unordered_map<int, std::vector<Edge>> po_cut_edges = cutting.MergePOAndCutAA();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 根据输入和规则建立空间索引 */
		std::cout << "----- Starting Space Index -----" << std::endl;
		SpaceIndex spaceIndex(input);
		spaceIndex.CreatSpaceIndexMerged();	// Via联通的两层合并建树
		spaceIndex.PrintSpaceIndexInfo();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 获取起点所在多边形id */
		std::cout << "----- Starting Get StartPos -----" << std::endl;
		int start_pos_s1_id = spaceIndex.GetStartPosinPolygonId(input.start_pos[0]);
		int start_pos_s2_id = spaceIndex.GetStartPosinPolygonId(input.start_pos[1]);
		std::cout << "StartPos s1 Polygon id:" << start_pos_s1_id << std::endl;
		std::cout << "StartPos s2 Polygon id:" << start_pos_s2_id << std::endl;
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 相交检测获取边集 */
		std::cout << "----- Starting Intersection Test -----" << std::endl;
		Intersect intersect(input, spaceIndex);
		std::vector<Edge> edges = intersect.getAllEdge();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 建图并求连通分量 */
		std::cout << "----- Starting Get Connected Component -----" << std::endl;
		Graph graph(input.total_polygon);
		graph.AddEdges(edges);
		// s1的联通分量，找到所有高电平PO
		std::vector<int> component_s1 = graph.GetConnectedComponent(start_pos_s1_id);
		// 对于高电平PO, 需要考虑增加其切割边
		for (auto& id : component_s1) {
			if (input.polygons[id]->layer_id == input.gate_rule.first) { // PO层
				auto it = po_cut_edges.find(id);
				if (it != po_cut_edges.end()) { // 有切割边则增加进去
					for (auto& e : it->second) {
						edges.emplace_back(e);
					}
				}
			}
		}
		// 重新建图并求s2的联通分量
		Graph new_graph(input.total_polygon);
		new_graph.AddEdges(edges);
		std::vector<int> component_s2 = new_graph.GetConnectedComponent(start_pos_s2_id);
		std::cout << "s1 connected polygon size: " << component_s1.size() << ", s2 connected polygon size: " << component_s2.size() << std::endl;
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		return component_s2;
	}
}

// BFS驱动延迟建图-单线程
std::vector<int> Trace::TraceUsingLazyGraph() {
    Timer myTimer;
    if (!input.has_gate_rule) // 若没有Gate规则(单起点)
    {
        /* 根据输入和规则建立空间索引 */
        std::cout << "----- Starting Space Index -----" << std::endl;
        SpaceIndex spaceIndex(input);
		spaceIndex.CreatSpaceIndexSeparated();	//每层独立建树索引
        spaceIndex.PrintSpaceIndexInfo();
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

        /* 获取起点所在多边形id */
        std::cout << "----- Starting Get StartPos -----" << std::endl;
        int start_pos_id = spaceIndex.GetStartPosinPolygonId(input.start_pos[0]);
        std::cout << "StartPos Polygon id:" << start_pos_id << std::endl;
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

        /* 懒惰 BFS 获取连通分量 */
        std::cout << "----- Starting Lazy BFS -----" << std::endl;
        Intersect intersect(input, spaceIndex);
        std::vector<bool> bfs_visted(input.total_polygon, false);
        std::vector<int> component = RunLazyConnectedComponent(spaceIndex, intersect, start_pos_id, bfs_visted);
		intersect.FlushStats();
        std::cout << "s1 connected polygon size: " << component.size() << std::endl;
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

        return component;
    }
	else // 有Gate规则(双起点)
	{
		/* PO层多边形合并与AA层多边形切割 */
		std::cout << "----- Starting Merge and Cutting Polygon -----" << std::endl;
		PolygonCutting cutting(input);
		robin_hood::unordered_map<int, std::vector<Edge>> po_cut_edges = cutting.MergePOAndCutAA();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 根据输入和规则建立空间索引 */
		std::cout << "----- Starting Space Index -----" << std::endl;
		SpaceIndex spaceIndex(input);
		spaceIndex.CreatSpaceIndexSeparated();	//每层独立建树索引
		spaceIndex.PrintSpaceIndexInfo();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 获取起点所在多边形id */
		std::cout << "----- Starting Get StartPos -----" << std::endl;
		int start_pos_s1_id = spaceIndex.GetStartPosinPolygonId(input.start_pos[0]);
		int start_pos_s2_id = spaceIndex.GetStartPosinPolygonId(input.start_pos[1]);
		std::cout << "StartPos s1 Polygon id:" << start_pos_s1_id << std::endl;
		std::cout << "StartPos s2 Polygon id:" << start_pos_s2_id << std::endl;
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		Intersect intersect(input, spaceIndex);
		std::vector<bool> bfs_visted(input.total_polygon, false);

		/* s1 懒惰 BFS */
		std::cout << "----- Starting Lazy BFS (s1) -----" << std::endl;
		std::vector<int> component_s1 = RunLazyConnectedComponent(spaceIndex, intersect, start_pos_s1_id, bfs_visted);
		intersect.FlushStats();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 构建额外切割边并执行 s2 懒惰 BFS */
		std::cout << "----- Starting Lazy BFS (s2) -----" << std::endl;
		robin_hood::unordered_map<int, std::vector<int>> extra_adj;
		extra_adj.reserve(po_cut_edges.size());
		for (auto& id : component_s1) {
			if (input.polygons[id]->layer_id != input.gate_rule.first) continue;
			auto it = po_cut_edges.find(id);
			if (it == po_cut_edges.end()) continue;
			for (auto& edge : it->second) {
				extra_adj[edge.first].push_back(edge.second);
				extra_adj[edge.second].push_back(edge.first);
			}
		}
		bfs_visted.assign(input.total_polygon, false);
		std::vector<int> component_s2 = RunLazyConnectedComponent(spaceIndex, intersect, start_pos_s2_id, bfs_visted, &extra_adj);
		intersect.FlushStats();
		std::cout << "s1 connected polygon size: " << component_s1.size() << ", s2 connected polygon size: " << component_s2.size() << std::endl;
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		return component_s2;
	}
}

// 先完全建图再BFS-多线程
std::vector<int> Trace::TraceUsingCompleteGraphParallel(int thread_count) {
	Timer myTimer;
	if (!input.has_gate_rule) // 若没有Gate规则(单起点)
	{
		/* 根据输入和规则建立空间索引 */
		std::cout << "----- Starting Space Index -----" << std::endl;
		SpaceIndex spaceIndex(input);
		spaceIndex.CreatSpaceIndexMergedParallel(thread_count);	// Via联通的两层合并建树
		spaceIndex.PrintSpaceIndexInfo();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 获取起点所在多边形id */
		std::cout << "----- Starting Get StartPos -----" << std::endl;
		int start_pos_id = spaceIndex.GetStartPosinPolygonIdParallel(input.start_pos[0], thread_count);
		std::cout << "StartPos Polygon id:" << start_pos_id << std::endl;
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 相交检测获取边集 */
		std::cout << "----- Starting Intersection Test -----" << std::endl;
		Intersect intersect(input, spaceIndex);
		std::vector<Edge> edges = intersect.getAllEdgeParallel(thread_count);
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 建图并求连通分量 */
		std::cout << "----- Starting Get Connected Component -----" << std::endl;
		Graph graph(input.total_polygon);
		graph.AddEdges(edges);
		std::vector<int> component = graph.GetConnectedComponent(start_pos_id);
		std::cout << "s1 connected polygon size: " << component.size() << std::endl;
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		return component;
	}
	else // 有Gate规则(双起点)
	{
		/* PO层多边形合并与AA层多边形切割 */
		std::cout << "----- Starting Merge and Cutting Polygon -----" << std::endl;
		PolygonCutting cutting(input);
		robin_hood::unordered_map<int, std::vector<Edge>> po_cut_edges = cutting.MergePOAndCutAA();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 根据输入和规则建立空间索引 */
		std::cout << "----- Starting Space Index -----" << std::endl;
		SpaceIndex spaceIndex(input);
		spaceIndex.CreatSpaceIndexMergedParallel(thread_count);	// Via联通的两层合并建树
		spaceIndex.PrintSpaceIndexInfo();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 获取起点所在多边形id */
		std::cout << "----- Starting Get StartPos -----" << std::endl;
		int start_pos_s1_id = spaceIndex.GetStartPosinPolygonIdParallel(input.start_pos[0], thread_count);
		int start_pos_s2_id = spaceIndex.GetStartPosinPolygonIdParallel(input.start_pos[1], thread_count);
		std::cout << "StartPos s1 Polygon id:" << start_pos_s1_id << std::endl;
		std::cout << "StartPos s2 Polygon id:" << start_pos_s2_id << std::endl;
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 相交检测获取边集 */
		std::cout << "----- Starting Intersection Test -----" << std::endl;
		Intersect intersect(input, spaceIndex);
		std::vector<Edge> edges = intersect.getAllEdgeParallel(thread_count);
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 建图并求连通分量 */
		std::cout << "----- Starting Get Connected Component -----" << std::endl;
		Graph graph(input.total_polygon);
		graph.AddEdges(edges);
		// s1的联通分量，找到所有高电平PO
		std::vector<int> component_s1 = graph.GetConnectedComponent(start_pos_s1_id);
		// 对于高电平PO, 需要考虑增加其切割边
		for (auto& id : component_s1) {
			if (input.polygons[id]->layer_id == input.gate_rule.first) { // PO层
				auto it = po_cut_edges.find(id);
				if (it != po_cut_edges.end()) { // 有切割边则增加进去
					for (auto& e : it->second) {
						edges.emplace_back(e);
					}
				}
			}
		}
		// 重新建图并求s2的联通分量
		Graph new_graph(input.total_polygon);
		new_graph.AddEdges(edges);
		std::vector<int> component_s2 = new_graph.GetConnectedComponent(start_pos_s2_id);
		std::cout << "s1 connected polygon size: " << component_s1.size() << ", s2 connected polygon size: " << component_s2.size() << std::endl;
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		return component_s2;
	}
}

// BFS驱动延迟建图-多线程
std::vector<int> Trace::TraceUsingLazyGraphParallel(int thread_count) {
	Timer myTimer;
    if (!input.has_gate_rule) // 若没有Gate规则(单起点)
    {
        /* 根据输入和规则建立空间索引 */
        std::cout << "----- Starting Space Index -----" << std::endl;
        SpaceIndex spaceIndex(input);
		spaceIndex.CreatSpaceIndexSeparatedParallel(thread_count);	//每层独立建树索引
        spaceIndex.PrintSpaceIndexInfo();
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

        /* 获取起点所在多边形id */
        std::cout << "----- Starting Get StartPos -----" << std::endl;
        int start_pos_id = spaceIndex.GetStartPosinPolygonIdParallel(input.start_pos[0], thread_count);
        std::cout << "StartPos Polygon id:" << start_pos_id << std::endl;
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

        /* 懒惰 BFS 获取连通分量 */
        std::cout << "----- Starting Lazy BFS -----" << std::endl;
        Intersect intersect(input, spaceIndex);
        std::vector<bool> bfs_visted(input.total_polygon, false);
        std::vector<int> component = RunLazyConnectedComponentParallel(spaceIndex, intersect, start_pos_id, bfs_visted, nullptr, thread_count);
		intersect.FlushStats();
        std::cout << "s1 connected polygon size: " << component.size() << std::endl;
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

        return component;
    }
	else // 有Gate规则(双起点)
	{
		/* PO层多边形合并与AA层多边形切割 */
		std::cout << "----- Starting Merge and Cutting Polygon -----" << std::endl;
		PolygonCutting cutting(input);
		robin_hood::unordered_map<int, std::vector<Edge>> po_cut_edges = cutting.MergePOAndCutAA();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 根据输入和规则建立空间索引 */
		std::cout << "----- Starting Space Index -----" << std::endl;
		SpaceIndex spaceIndex(input);
		spaceIndex.CreatSpaceIndexSeparatedParallel(thread_count);	//每层独立建树索引
		spaceIndex.PrintSpaceIndexInfo();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 获取起点所在多边形id */
		std::cout << "----- Starting Get StartPos -----" << std::endl;
		int start_pos_s1_id = spaceIndex.GetStartPosinPolygonIdParallel(input.start_pos[0], thread_count);
		int start_pos_s2_id = spaceIndex.GetStartPosinPolygonIdParallel(input.start_pos[1], thread_count);
		std::cout << "StartPos s1 Polygon id:" << start_pos_s1_id << std::endl;
		std::cout << "StartPos s2 Polygon id:" << start_pos_s2_id << std::endl;
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		Intersect intersect(input, spaceIndex);
		std::vector<bool> bfs_visted(input.total_polygon, false);

		/* s1 懒惰 BFS */
		std::cout << "----- Starting Lazy BFS (s1) -----" << std::endl;
		std::vector<int> component_s1 = RunLazyConnectedComponentParallel(spaceIndex, intersect, start_pos_s1_id, bfs_visted, nullptr, thread_count);
		intersect.FlushStats();
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		/* 构建额外切割边并执行 s2 懒惰 BFS */
		std::cout << "----- Starting Lazy BFS (s2) -----" << std::endl;
		robin_hood::unordered_map<int, std::vector<int>> extra_adj;
		extra_adj.reserve(po_cut_edges.size());
		for (auto& id : component_s1) {
			if (input.polygons[id]->layer_id != input.gate_rule.first) continue;
			auto it = po_cut_edges.find(id);
			if (it == po_cut_edges.end()) continue;
			for (auto& edge : it->second) {
				extra_adj[edge.first].push_back(edge.second);
				extra_adj[edge.second].push_back(edge.first);
			}
		}
		bfs_visted.assign(input.total_polygon, false);
		std::vector<int> component_s2 = RunLazyConnectedComponentParallel(spaceIndex, intersect, start_pos_s2_id, bfs_visted, &extra_adj, thread_count);
		intersect.FlushStats();
		std::cout << "s1 connected polygon size: " << component_s1.size() << ", s2 connected polygon size: " << component_s2.size() << std::endl;
		std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

		return component_s2;
	}
}

// Lazy BFS：仅按需扩展起点可达区域
std::vector<int> Trace::RunLazyConnectedComponent(SpaceIndex& spaceIndex, Intersect& intersect, int start_id,
	std::vector<bool>& bfs_visted, const robin_hood::unordered_map<int, std::vector<int>>* extra_adj) {
	if (start_id < 0 || start_id >= input.total_polygon) return {};

	const Polygon* start_poly = input.polygons[start_id];
	const std::string& start_layer_name = start_poly ? input.layer_id_to_name[start_poly->layer_id] : std::string("unknown");

	std::cout << "[LazyBFS] start id=" << start_id << " layer=" << start_layer_name << std::endl;

	std::queue<int> bfs_queue;
	bfs_queue.push(start_id);
	bfs_visted[start_id] = true;

	std::vector<int> component;
	component.reserve(1000000);
	std::vector<int> neighbors;
	neighbors.reserve(100);

	size_t expansions = 0;
	size_t neighbor_candidates = 0;
	size_t extra_neighbors = 0;
	size_t skipped_visited = 0;
	size_t enqueued = 0;

	while (!bfs_queue.empty()) {
		int current = bfs_queue.front();
		bfs_queue.pop();
		component.push_back(current);
		++expansions;

		// 基于当前多边形id获取所有邻居多边形
		neighbors.clear();
		intersect.GetNeighborsLazy(current, bfs_visted, neighbors);
		neighbor_candidates += neighbors.size();

		// 是否有额外切割边需要加入
		if (extra_adj) {
			auto it = extra_adj->find(current);
			if (it != extra_adj->end()) {
				extra_neighbors += it->second.size();
				neighbors.insert(neighbors.end(), it->second.begin(), it->second.end());
			}
		}

		// 未访问邻居入队
		for (int next : neighbors) {
			if (!bfs_visted[next]) {
				bfs_visted[next] = true;
				bfs_queue.push(next);
				++enqueued;
			}
			else {
				++skipped_visited;
			}
		}
	}

	std::cout << "[LazyBFS] summary start_id=" << start_id
		<< " visited=" << component.size()
		<< " expansions=" << expansions
		<< " neighbors=" << neighbor_candidates
		<< " extra_neighbors=" << extra_neighbors
		<< " skipped_visited=" << skipped_visited
		<< " enqueued=" << enqueued << std::endl;

	return component;
}

// 并行版本：Lazy BFS：仅按需扩展起点可达区域
std::vector<int> Trace::RunLazyConnectedComponentParallel(SpaceIndex& spaceIndex, Intersect& intersect, int start_id,
	std::vector<bool>& bfs_visted, const robin_hood::unordered_map<int, std::vector<int>>* extra_adj, int thread_count) {
	if (start_id < 0 || start_id >= input.total_polygon) return {};

	const Polygon* start_poly = input.polygons[start_id];
	const std::string& start_layer_name = start_poly ? input.layer_id_to_name[start_poly->layer_id] : std::string("unknown");

	std::cout << "[LazyBFS] start id=" << start_id << " layer=" << start_layer_name << std::endl;

	std::queue<int> bfs_queue;
	bfs_queue.push(start_id);
	bfs_visted[start_id] = true;

	std::vector<int> component;
	component.reserve(1000000);

	size_t expansions = 0;
	size_t neighbor_candidates = 0;
	size_t extra_neighbors = 0;
	size_t skipped_visited = 0;
	size_t enqueued = 0;

	// 复用数据结构
	std::vector<int> current_level;
	std::vector<std::vector<int>> neighbors_list;
	neighbors_list.reserve(1000);  // 预分配大致容量

	omp_set_num_threads(thread_count);
	while (!bfs_queue.empty()) {
	    // 处理当前层级的所有节点
	    size_t level_size = bfs_queue.size();
	    expansions += level_size;
	
	    // 准备当前层级节点
    	current_level.clear();
    	current_level.reserve(level_size);
		
    	// 调整邻居列表大小（复用已有向量）
    	if (neighbors_list.size() < level_size) {
    	    size_t prev_size = neighbors_list.size();
    	    neighbors_list.resize(level_size);
    	    // 为新元素预分配内存
    	    for (size_t i = prev_size; i < level_size; ++i) {
    	        neighbors_list[i].reserve(100);
    	    }
    	}
	
	    // 出队当前层级节点
	    for (size_t i = 0; i < level_size; ++i) {
	        int current = bfs_queue.front();
	        bfs_queue.pop();
	        current_level.push_back(current);
	        component.push_back(current);
	    }

	    // 并行获取邻居 (使用 OpenMP) ：动态调度小批次任务并行，负载均衡
	   	#pragma omp parallel for schedule(dynamic, 8) reduction(+:neighbor_candidates, extra_neighbors)
	    for (size_t i = 0; i < level_size; ++i) {
			int current = current_level[i];
	        // 复用邻居向量（保留容量）
        	neighbors_list[i].clear();
	        // 获取基础邻居
	        intersect.GetNeighborsLazyParallel(current, bfs_visted, neighbors_list[i]);
	        neighbor_candidates += neighbors_list[i].size();
	        // 添加额外邻居
	        if (extra_adj) {
	            auto it = extra_adj->find(current);
	            if (it != extra_adj->end()) {
	                const auto& extra = it->second;
	                extra_neighbors += extra.size();
	                neighbors_list[i].insert(neighbors_list[i].end(), extra.begin(), extra.end());
	            }
	        }
		}

		// 处理邻居节点
	    for (size_t i = 0; i < level_size; ++i) {
	        for (int next : neighbors_list[i]) {
	            if (!bfs_visted[next]) {
	                bfs_visted[next] = true;
	                bfs_queue.push(next);
	                ++enqueued;
	            } else {
	                ++skipped_visited;
	            }
	        }
	    }
	}

	std::cout << "[LazyBFS] summary start_id=" << start_id
		<< " visited=" << component.size()
		<< " expansions=" << expansions
		<< " neighbors=" << neighbor_candidates
		<< " extra_neighbors=" << extra_neighbors
		<< " skipped_visited=" << skipped_visited
		<< " enqueued=" << enqueued << std::endl;

	return component;
}


#pragma endregion
