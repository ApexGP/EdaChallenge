#pragma once
#include "public.h"
#include "QuadTree.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <set>

class Input {
public:
    Rect layout;                                                    // 版图布局边界框
    robin_hood::unordered_map<std::string, int> layer_name_to_id;   // 图层名转换id, 0-index, 后续使用id索引
    robin_hood::unordered_map<int, std::string> layer_id_to_name;   // 图层id转名称，用于输出
    int total_polygon;                                              // 多边形总数
    std::vector<Polygon*> polygons;                                 // 所有多边形的列表，下标对应多边形id, 0-index
    std::vector<Range> polygon_id_range_in_layer;                   // 每层的多边形id范围，双闭区间，下标对应图层id
    std::set<Edge> via_rules;                                       // Via规则集合，表示哪些层可以通孔，通过排序保证id小的在前确保set不重复
    bool has_gate_rule;                                             // 是否存在Gate规则
    Edge gate_rule;                                                 // 元组表示Gate规则, gate_rule.first为Poly层，gate_rule.second为AA层
    std::vector<StartPos> start_pos;                                // 起始位置，可能多个，按输入顺序


    // 根据文件路径初始化读取数据
    Input(std::string layout_path, std::string rule_path) {
        polygons.reserve(1000000); // 预分配一些空间
        layout = Rect(INT_MAX, INT_MAX, INT_MIN, INT_MIN);
        has_gate_rule = false;

        std::ifstream layout_file(layout_path); // 打开版图文件
        assert(layout_file.is_open() && "无法打开layout文件");
        readLayout(layout_file);
        layout_file.close();

        std::ifstream rule_file(rule_path);
        assert(rule_file.is_open() && "无法打开rule文件");
        readRule(rule_file);
        rule_file.close();
    }

    ~Input() {
        for (size_t i = 0; i < polygons.size(); i++) {
            if (polygons[i]) delete polygons[i];
        }
    }

    // 读取版图文件
    void readLayout(std::ifstream& layout_file) {
        // 设置10MB的缓冲区
        constexpr size_t bufferSize = 1024 * 1024 * 10;
        std::vector<char> buffer(bufferSize);
        layout_file.rdbuf()->pubsetbuf(buffer.data(), bufferSize);

        std::string line;
        int layer_id = -1;
        int polygon_id = -1;

        while (std::getline(layout_file, line)) {
            if (line != "" && line[0] != '(') { // 图层行
                layer_id++;
                polygon_id_range_in_layer.emplace_back(polygon_id + 1, 0); // 新建层，已知起始多边形id
                if (layer_id != 0) {
                    polygon_id_range_in_layer[layer_id - 1].second = polygon_id; // 已知上一层末尾多边形id
                }
                layer_name_to_id[line] = layer_id;
                layer_id_to_name[layer_id] = line;
            }
            else { // 多边形行
                polygon_id++;
                Polygon* poly = new Polygon();
                poly->id = polygon_id;
                poly->layer_id = layer_id;

                // 将字符串解析为坐标点存入istringstream
                fastParseCoordinates(line, poly->vertex);
                // 计算多边形的矩形包围盒
                Rect poly_rect = GetRectofPolygon(poly);
                poly->rect = poly_rect;

                // 更新版图边界
                layout.update(poly_rect);
                // 存储多边形
                polygons.emplace_back(std::move(poly));
            }
        }
        polygon_id_range_in_layer[layer_id].second = polygon_id; // 最后一层末尾多边形id
        total_polygon = polygon_id + 1;
    }

    // 读取规则文件
    void readRule(std::ifstream& rule_file) {
        std::istringstream iss;
        std::string line;
        std::string flag;
        char c;

        while (std::getline(rule_file, line)) {
            if (line == "StartPos" || line == "Via" || line == "Gate") {
                flag = line;
            }
            else {
                iss.clear();          // 清除流状态
                iss.str(line);        // 设置字符串为流

                if (flag == "StartPos") { // 起始位置
                    StartPos startpos;
                    std::string layer_name;
                    iss >> layer_name;
                    startpos.first = layer_name_to_id[layer_name];
                    iss >> c >> startpos.second.first >> c >> startpos.second.second >> c;
                    start_pos.emplace_back(startpos);
                }
                else if (flag == "Via") { // 处理Via规则
                    std::string first_layer, second_layer;
                    iss >> first_layer;
                    while (iss >> second_layer) {
                        int fid = layer_name_to_id[first_layer];
                        int sid = layer_name_to_id[second_layer];
                        if (fid > sid) { // 保证小的层id在前
                            std::swap(fid, sid);
                        }
                        via_rules.insert(Edge(fid, sid));

                        first_layer = second_layer; // 确保连续传递性
                    }
                }
                else if (flag == "Gate") { // 处理Gate规则
                    has_gate_rule = true;
                    std::string first_layer, second_layer;
                    iss >> first_layer >> second_layer;
                    int fid = layer_name_to_id[first_layer];
                    int sid = layer_name_to_id[second_layer];
                    gate_rule = Edge(fid, sid);
                }
            }
        }
    }

    // 打印版图信息
    void PrintLayoutInfo() {
        // check print
        std::cout << "[Layout]" << std::endl;
        std::cout << "total polygon num:" << total_polygon << std::endl;
        std::cout << "total layer num:" << polygon_id_range_in_layer.size() << std::endl;
        for (auto& name_id : layer_name_to_id) {
            std::cout << "layer name:" << name_id.first << " id:" << name_id.second
                << " polygon num:" << polygon_id_range_in_layer[name_id.second].second - polygon_id_range_in_layer[name_id.second].first + 1
                << " polygon id range:[" << polygon_id_range_in_layer[name_id.second].first << "," << polygon_id_range_in_layer[name_id.second].second << "]" << std::endl;
        }
    }

    // 打印规则信息
    void PrintRuleInfo() {
        // check print
        std::cout << "[Rule]" << std::endl;
        for (auto& st : start_pos)
            std::cout << "StartPos:" << layer_id_to_name[st.first] << " " << st.second.first << " " << st.second.second << std::endl;
        for (auto& vi : via_rules)
            std::cout << "Via:" << layer_id_to_name[vi.first] << " " << layer_id_to_name[vi.second] << std::endl;
        if (has_gate_rule)
            std::cout << "Gate:" << layer_id_to_name[gate_rule.first] << " " << layer_id_to_name[gate_rule.second] << std::endl;
        else
            std::cout << "no Gate" << std::endl;
    }

    // 获取多边形的矩形包围盒
    Rect GetRectofPolygon(Polygon* poly) {
        // 使用迭代器遍历多边形顶点
        auto vertex_iter = poly->vertex.begin();
        auto vertex_end = poly->vertex.end();

        // 初始化边界值
        const int first_x = vertex_iter->x;
        const int first_y = vertex_iter->y;

        int xmin = first_x;
        int xmax = xmin;
        int ymin = first_y;
        int ymax = ymin;

        ++vertex_iter; // 跳过第一个顶点

        // 一次遍历找到所有边界
        for (; vertex_iter != vertex_end; ++vertex_iter) {
            int x = vertex_iter->x;
            int y = vertex_iter->y;

            // 使用三元运算符减少if-else
            xmin = (x < xmin) ? x : xmin;
            xmax = (x > xmax) ? x : xmax;
            ymin = (y < ymin) ? y : ymin;
            ymax = (y > ymax) ? y : ymax;
        }

        return Rect(xmin, ymin, xmax, ymax);
    }

private:
    // 快速解析坐标字符串，使用字符指针代替istringstream
    void fastParseCoordinates(const std::string& line, Vertexs& vertex) {
        vertex.reserve(12); // 预分配每个多边形12个顶点
        const char* ptr = line.c_str();
        const char* end = ptr + line.length();

        while (ptr < end) {
            // 寻找第一个 '('
            while (ptr < end && *ptr != '(') ptr++;
            if (ptr >= end) break;

            ptr++; // 跳过 '('

            // 解析x坐标
            int x = 0;
            bool x_negative = false;
            if (*ptr == '-') {
                x_negative = true;
                ptr++;
            }
            while (ptr < end && *ptr >= '0' && *ptr <= '9') {
                x = x * 10 + (*ptr - '0');
                ptr++;
            }
            if (x_negative) x = -x;

            // 跳过逗号
            while (ptr < end && *ptr != ',') ptr++;
            if (ptr >= end) break;
            ptr++; // 跳过 ','

            // 解析y坐标
            int y = 0;
            bool y_negative = false;
            if (*ptr == '-') {
                y_negative = true;
                ptr++;
            }
            while (ptr < end && *ptr >= '0' && *ptr <= '9') {
                y = y * 10 + (*ptr - '0');
                ptr++;
            }
            if (y_negative) y = -y;

            // 添加点到顶点列表
            vertex.push_back(MPoint_2(x, y));

            // 寻找对应的 ')'
            while (ptr < end && *ptr != ')') ptr++;
            if (ptr < end) ptr++; // 跳过 ')'
        }
    }
};