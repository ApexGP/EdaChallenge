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
    Intersect intersection(input, spaceIndex);
    std::vector<Edge> edges = intersection.getAllEdge();
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
        string layout_path = argv[2];
        string rule_path = argv[4];
        int thread = 1;
        string res_path;
        if (strcmp(argv[5], "-thread")) {
            thread = atoi(argv[6]);
            res_path = argv[8];
        }
        else {
            res_path = argv[6];
        }

        solve(layout_path, rule_path, thread, res_path);
    }
    else {
        // for self-test.
        string dir_path = "C:/Users/PC/Desktop/EdaChallenge/";
        string layout_path = dir_path + "instance/case/case1_small_layout.txt";
        string rule_path = dir_path + "instance/Rule/public_small_rule2.txt";

        size_t pos = layout_path.find_last_of('/');
        std::string case_name = (pos == std::string::npos) ? layout_path: layout_path.substr(pos + 1);
        case_name = case_name.substr(0, case_name.size() - 4);

        string res_path = dir_path + "solution/" + case_name + "_q" + rule_path.substr(rule_path.size() - 5);
        int thread = 1;

        solve(layout_path, rule_path, thread, res_path);
    }
    return 0;
}


