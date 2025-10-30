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

    ~Input() {}

    // 读取版图文件 - 高效版本（一次性读入）
    void readLayout(std::ifstream& layout_file) {
        // 获取文件大小
        layout_file.seekg(0, std::ios::end);
        size_t file_size = layout_file.tellg();
        layout_file.seekg(0, std::ios::beg);

        // 一次性读取整个文件到内存
        std::vector<char> file_buffer(file_size + 1); // 多一个字节用于字符串终止
        layout_file.read(file_buffer.data(), file_size);
        file_buffer[file_size] = '\0'; // 添加字符串终止符

        // 第一步：预估多边形数量（统计行数）
        size_t estimated_polygons = 0;
        char* count_ptr = file_buffer.data();
        char* count_end = file_buffer.data() + file_size;
        
        while (count_ptr < count_end) {
            if (*count_ptr == '\n') {
                estimated_polygons++;
            }
            count_ptr++;
        }
        // 预留空间
        polygons.reserve(estimated_polygons);

        int layer_id = -1;
        int polygon_id = -1;

        char* start = file_buffer.data();
        char* end = start + file_size;
        char* current = start;

        // 手动解析行
         while (current < end) {
            char* line_start = current;  // 记录当前行的起始位置
                    
            // 快速查找行结束：遍历直到遇到换行符或文件结尾
            while (current < end && *current != '\n' && *current != '\r') {
                current++;
            }
            
            // 计算当前行的长度（从行开始到当前指针位置）
            size_t line_len = current - line_start;
            
            // 处理非空行
            if (line_len > 0) {
                // 判断行类型：不以'('开头的是图层行
                if (line_start[0] != '(') {
                    layer_id++;
                    polygon_id_range_in_layer.emplace_back(polygon_id + 1, 0);
                    // 如果不是第一个图层，设置前一图层的结束多边形ID
                    if (layer_id != 0) {
                        polygon_id_range_in_layer[layer_id - 1].second = polygon_id;
                    }
                    // 创建图层名称字符串并建立双向映射
                    std::string layer_name(line_start, line_len);
                    layer_name_to_id[layer_name] = layer_id;
                    layer_id_to_name[layer_id] = layer_name;
                } else {
                    // 多边形行处理逻辑
                    polygon_id++;
                    Polygon* poly = new Polygon();
                    poly->id = polygon_id;
                    poly->layer_id = layer_id;
                
                    // 解析坐标数据
                    fastParseCoordinates(line_start, line_start + line_len, poly->vertex);
                    // 计算多边形的包围盒矩形
                    Rect poly_rect = GetRectofPolygon(poly);
                    poly->rect = poly_rect;
                
                    // 更新整个版图的边界范围
                    layout.update(poly_rect);
                    // 将多边形添加到容器中
                    polygons.emplace_back(poly);
                }
            }
            
            // 跳过行结束符：处理连续的回车换行符（兼容不同平台的换行格式）
            while (current < end && (*current == '\n' || *current == '\r')) current++;
        }

        polygon_id_range_in_layer[layer_id].second = polygon_id;
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
    void fastParseCoordinates(const char* line_start, const char* line_end, Vertexs& vertex) {
        vertex.reserve(12); // 预分配每个多边形12个顶点
        const char* ptr = line_start;
        const char* end = line_end;

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