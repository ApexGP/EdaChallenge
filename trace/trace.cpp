#include "public.h"
#include "Input.hpp"
#include "SpaceIndex.hpp"
#include "Intersect.hpp"
#include "Graph.hpp"
#include "Output.hpp"
using namespace std;
using namespace std::chrono;

steady_clock::time_point startWallClockTime = steady_clock::now();

// 获取已运行时间s
static double getPastSecond() { 
    return duration_cast<duration<double>>(steady_clock::now() - startWallClockTime).count();
};

// 求解
static void solve(string layout_path, string rule_path, int thread, string res_path) {
    double myStartTime = getPastSecond();
     /* 输入 */
    std::cout << "----- Starting Input -----" << std::endl;
    Input input(layout_path, rule_path);
    input.PrintLayoutInfo();
    input.PrintRuleInfo();
    std::cout << "----- Use Time: " << getPastSecond() - myStartTime << " s" << std::endl << std::endl;

    myStartTime = getPastSecond();
    /* 根据输入和规则建立空间索引 */
    std::cout << "----- Starting Space Index -----" << std::endl;
    SpaceIndex spaceIndex(input);
    spaceIndex.PrintSpaceIndexInfo();
    std::cout << "----- Use Time: " << getPastSecond() - myStartTime << " s" << std::endl << std::endl;

    myStartTime = getPastSecond();
     /* 获取起点所在多边形id */
    std::cout << "----- Starting Get StartPos -----" << std::endl;
    int start_pos_id = spaceIndex.GetStartPosinPolygonId(input.start_pos[0]);
    std::cout << "StartPos Polygon id:" << start_pos_id << std::endl;
    std::cout << "----- Use Time: " << getPastSecond() - myStartTime << " s" << std::endl << std::endl;

    myStartTime = getPastSecond();
    /* 相交检测获取边集 */
    std::cout << "----- Starting Intersection Test -----" << std::endl;
    //Intersect intersection(input, spaceIndex);
    Intersect intersect(input, spaceIndex);
    std::vector<Edge> edges = intersect.getAllEdge();
    std::cout << "----- Use Time: " << getPastSecond() - myStartTime << " s" << std::endl << std::endl;

    myStartTime = getPastSecond();
     /* 建图并求连通分量 */
    std::cout << "----- Starting Get Connected Component -----" << std::endl;
    Graph graph(input.total_polygon);
    graph.AddEdges(edges);
    std::vector<int> component = graph.GetConnectedComponent(start_pos_id);
    std::cout << "----- Use Time: " << getPastSecond() - myStartTime << " s" << std::endl << std::endl;

    myStartTime = getPastSecond();
     /* 输出 */
    std::cout << "----- Starting Output -----" << std::endl;
    Output output(input, res_path, component);
    std::cout << "----- Use Time: " << getPastSecond() - myStartTime << " s" << std::endl << std::endl;

    std::cout << "----- Total Time: " << getPastSecond() << " s" << std::endl;
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
        string dir_path = "C:/Users/PC/Desktop/EdaChallenge/";
        string layout_path = dir_path + "instance/case/case1_small_layout.txt";
        //string layout_path = dir_path + "instance/case/case1_large_0909b_layout.txt";
        string rule_path = dir_path + "instance/Rule/public_small_rule1.txt";
        //string rule_path = dir_path + "instance/Rule/public_large_rule2.txt";

        size_t pos = layout_path.find_last_of('/');
        std::string case_name = (pos == std::string::npos) ? layout_path: layout_path.substr(pos + 1);
        case_name = case_name.substr(0, case_name.size() - 4);

        string output_path = dir_path + "solution/" + case_name + "_q" + rule_path.substr(rule_path.size() - 5);
        int thread_count = 1;

        solve(layout_path, rule_path, thread_count, output_path);
    }
    return 0;
}


