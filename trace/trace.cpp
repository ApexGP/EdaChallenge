#include "public.h"
#include "Input.hpp"
#include "PolygonCutting.hpp"
#include "SpaceIndex.hpp"
#include "Intersect.hpp"
#include "Graph.hpp"
#include "Output.hpp"
#include <queue>
#include <limits>
#include <algorithm>
using namespace std;
using namespace std::chrono;

// 懒惰BFS：仅按需扩展起点可达区域
static std::vector<int> RunLazyConnectedComponent(Input& input, SpaceIndex& spaceIndex, Intersect& intersect, int start_id,
	std::vector<int>& visit_token, int& visit_version,
	const robin_hood::unordered_map<int, std::vector<int>>* extra_adj = nullptr) {
	if (start_id < 0 || start_id >= input.total_polygon) {
		return {};
	}

	if (visit_token.size() != static_cast<size_t>(input.total_polygon)) {
		visit_token.assign(input.total_polygon, 0);
		visit_version = 0;
	} else if (visit_version == std::numeric_limits<int>::max()) {
		std::fill(visit_token.begin(), visit_token.end(), 0);
		visit_version = 0;
	}

	const Polygon* start_poly = input.polygons[start_id];
	const std::string& start_layer_name = start_poly ? input.layer_id_to_name[start_poly->layer_id] : std::string("unknown");

	visit_version += 1;
	std::cout << "[LazyBFS] start id=" << start_id
		<< " layer=" << start_layer_name
		<< " version=" << visit_version << std::endl;

	std::queue<int> bfs_queue;
	bfs_queue.push(start_id);
	visit_token[start_id] = visit_version;

	std::vector<int> component;
	component.reserve(1024);
	std::vector<int> neighbors;
	neighbors.reserve(64);

	size_t expansions = 0;
	size_t neighbor_candidates = 0;
	size_t extra_neighbors = 0;
	size_t skipped_invalid = 0;
	size_t skipped_visited = 0;
	size_t enqueued = 0;

	while (!bfs_queue.empty()) {
		int current = bfs_queue.front();
		bfs_queue.pop();
		component.push_back(current);
		++expansions;

		neighbors.clear();
		intersect.GetNeighborsLazy(current, visit_token, visit_version, neighbors);
		neighbor_candidates += neighbors.size();

		if (extra_adj) {
			auto it = extra_adj->find(current);
			if (it != extra_adj->end()) {
				extra_neighbors += it->second.size();
				neighbors.insert(neighbors.end(), it->second.begin(), it->second.end());
			}
		}

		for (int next : neighbors) {
			if (next < 0 || next >= input.total_polygon) {
				++skipped_invalid;
				continue;
			}

			if (visit_token[next] == visit_version) {
				++skipped_visited;
				continue;
			}

			visit_token[next] = visit_version;
			bfs_queue.push(next);
			++enqueued;
		}
	}

	std::cout << "[LazyBFS] summary id=" << start_id
		<< " visited=" << component.size()
		<< " expansions=" << expansions
		<< " neighbors=" << neighbor_candidates
		<< " extra_neighbors=" << extra_neighbors
		<< " skipped_invalid=" << skipped_invalid
		<< " skipped_visited=" << skipped_visited
		<< " enqueued=" << enqueued << std::endl;

	return component;
}


// 求解
static void solve(string layout_path, string rule_path, int thread, string res_path) {
	Timer myTimer;
	 /* 输入 */
    std::cout << "----- Starting Input -----" << std::endl;
    Input input(layout_path, rule_path);
    input.PrintLayoutInfo();
    input.PrintRuleInfo();
    std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

    if (!input.has_gate_rule) // 若没有Gate规则(单起点)
    { 
        /* 根据输入和规则建立空间索引 */
        std::cout << "----- Starting Space Index -----" << std::endl;
        SpaceIndex spaceIndex(input);
        spaceIndex.PrintSpaceIndexInfo();
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

        /* 获取起点所在多边形id */
        std::cout << "----- Starting Get StartPos -----" << std::endl;
        int start_pos_id = spaceIndex.GetStartPosinPolygonId(input.start_pos[0]);
        std::cout << "StartPos Polygon id:" << start_pos_id << std::endl;
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

        /* 懒惰 BFS 获取连通分量 */
        std::cout << "----- Starting Lazy BFS -----" << std::endl;
        std::vector<int> component;
        {
            Intersect intersect(input, spaceIndex);
            std::vector<int> visit_token;
            int visit_version = 0;
            component = RunLazyConnectedComponent(input, spaceIndex, intersect, start_pos_id, visit_token, visit_version);
        }
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

        /* 输出 */
        std::cout << "----- Starting Output -----" << std::endl;
        std::cout << "s1 connected polygon size: " << component.size() << std::endl;
        Output output(input, res_path, component);
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;
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
        spaceIndex.PrintSpaceIndexInfo();
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

        /* 获取起点所在多边形id */
        std::cout << "----- Starting Get StartPos -----" << std::endl;
        int start_pos_s1_id = spaceIndex.GetStartPosinPolygonId(input.start_pos[0]);
        int start_pos_s2_id = spaceIndex.GetStartPosinPolygonId(input.start_pos[1]);
        std::cout << "StartPos s1 Polygon id:" << start_pos_s1_id << std::endl;
        std::cout << "StartPos s2 Polygon id:" << start_pos_s2_id << std::endl;
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

        std::vector<int> component_s1;
        std::vector<int> component_s2;
        {
            Intersect intersect(input, spaceIndex);
            std::vector<int> visit_token;
            int visit_version = 0;

            /* s1 懒惰 BFS */
            std::cout << "----- Starting Lazy BFS (s1) -----" << std::endl;
            component_s1 = RunLazyConnectedComponent(input, spaceIndex, intersect, start_pos_s1_id, visit_token, visit_version);
            std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

            visit_token.assign(input.total_polygon, 0);
            visit_version = 0;

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
            component_s2 = RunLazyConnectedComponent(input, spaceIndex, intersect, start_pos_s2_id, visit_token, visit_version, &extra_adj);
        }
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;

        /* 输出 */
        std::cout << "----- Starting Output -----" << std::endl;
        std::cout << "s1 connected polygon size: " << component_s1.size() << ", s2 connected polygon size: " << component_s2.size() << std::endl;
        Output output(input, res_path, component_s2);
        std::cout << "----- Use Time: " << myTimer.FromLastCallElapsed() << " s" << std::endl << std::endl;
    }

    std::cout << "----- Total Time: " << myTimer.Elapsed() << " s" << std::endl;
}


int main(int argc, char* argv[])
{
    if (argc > 2) { // 命令行运行
        int thread_count = 1;  // 默认单线程
        const char* layout_path = nullptr;
        const char* rule_path = nullptr;
        const char* output_path = nullptr;

        // 顺序无关参数解析
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-layout") == 0 && i + 1 < argc) { // strcmp 相等时返回0 T_T
                layout_path = argv[++i];
            } else if (strcmp(argv[i], "-rule") == 0 && i + 1 < argc) {
                rule_path = argv[++i];
            } else if (strcmp(argv[i], "-thread") == 0 && i + 1 < argc) {
                thread_count = atoi(argv[++i]);
            } else if (strcmp(argv[i], "-output") == 0 && i + 1 < argc) {
                output_path = argv[++i];
            }
        }

        // 验证必需参数
        if (!layout_path || !rule_path || !output_path) {
            std::cerr << "错误：缺少必需参数\n";
            std::cerr << "用法: " << argv[0] 
                      << " -layout <文件> -rule <文件> [-thread <n>] -output <文件>\n";
            return 1;
        }

        // 验证线程数
        if (thread_count <= 0) {
            std::cerr << "警告：无效的线程数，使用默认值1\n";
            thread_count = 1;
        }

        // 使用参数...
        // std::cout << "布局文件: " << layout_path << "\n";
        // std::cout << "规则文件: " << rule_path << "\n";
        // std::cout << "输出文件: " << output_path << "\n";
        // std::cout << "线程数: " << thread_count << "\n";

        solve(layout_path, rule_path, thread_count, output_path);
    }
    else {
        // for self-test.
        string dir_path = "C:/Users/POJO/Desktop/EdaChallenge/";
        //string layout_path = dir_path + "instance/case/case1_small_layout.txt";
        string layout_path = dir_path + "instance/case/case1_large_0909b_layout.txt";
        //string rule_path = dir_path + "instance/Rule/public_small_rule3.txt";
        string rule_path = dir_path + "instance/Rule/public_large_rule3.txt";

        size_t pos = layout_path.find_last_of('/');
        std::string case_name = (pos == std::string::npos) ? layout_path: layout_path.substr(pos + 1);
        case_name = case_name.substr(0, case_name.size() - 4);

        string output_path = dir_path + "solution/" + case_name + "_q" + rule_path.substr(rule_path.size() - 5);
        int thread_count = 1;

        solve(layout_path, rule_path, thread_count, output_path);
    }
    return 0;
}


