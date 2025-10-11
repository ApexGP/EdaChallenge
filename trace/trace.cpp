#include "public.h"
#include "Input.hpp"
#include "PolygonCutting.hpp"
#include "SpaceIndex.hpp"
#include "Intersect.hpp"
#include "Graph.hpp"
#include "Output.hpp"
using namespace std;
using namespace std::chrono;

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
        string dir_path = "C:/Users/lenovo/Desktop/EdaChallenge/";
        string layout_path = dir_path + "instance/case/case1_small_layout.txt";
        //string layout_path = dir_path + "instance/case/case1_large_0909b_layout.txt";
        string rule_path = dir_path + "instance/Rule/public_small_rule3.txt";
        //string rule_path = dir_path + "instance/Rule/public_large_rule3.txt";

        size_t pos = layout_path.find_last_of('/');
        std::string case_name = (pos == std::string::npos) ? layout_path: layout_path.substr(pos + 1);
        case_name = case_name.substr(0, case_name.size() - 4);

        string output_path = dir_path + "solution/" + case_name + "_q" + rule_path.substr(rule_path.size() - 5);
        int thread_count = 1;

        solve(layout_path, rule_path, thread_count, output_path);
    }
    return 0;
}


