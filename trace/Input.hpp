#pragma once
#include "public.h"
#include "QuadTree.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <set>

class Input {
public:
	Rect layout;													// 版图的最大矩形框
	robin_hood::unordered_map<std::string, int> layer_name_to_id;   // 层名转层id, 0-index, 代码内使用id指代层
	robin_hood::unordered_map<int, std::string> layer_id_to_name;   // 层id转层名, 方便输出
	int total_polygon;												// 多边形总数
	std::vector<Polygon*> polygons;									// 所有的多边形列表, 下标对应其多边形id, 0-index
	std::vector<Range> polygon_id_range_in_layer;					// 每层的多边形的id范围, 双闭区间, 下标对应其层id
	std::set<Edge> via_rules;										// Via规则集合，表示哪两层连通, 通过输入保证层id小的在前，结合set确保不重复
	bool has_gate_rule;												// 是否存在Gate规则
	Edge gate_rule;													// 若存在则表示Gate规则, gate_rule.first为Poly层，gate_rule.second为AA层
	std::vector<StartPos> start_pos;								// 起点位置, 至少一个, 最多两个, 按输入顺序


	// 给定文件路径，获取输入
	Input(std::string layout_path, std::string rule_path) {
		polygons.reserve(1000000); // 预分配一些空间
		layout = Rect(INT_MAX, INT_MAX, INT_MIN, INT_MIN);
		has_gate_rule = false;

		std::ifstream layout_file(layout_path); // 创建并打开文件
		assert(layout_file.is_open() && "无法打开layout文件");
		readLayout(layout_file);
		layout_file.close();

		std::ifstream rule_file(rule_path);
		assert(rule_file.is_open() && "无法打开rule文件");
		readRule(rule_file);
		rule_file.close();
	}
	~Input() {
		for (size_t i = 0; i < polygons.size(); i++){
			if (polygons[i]) delete polygons[i];
		}
	}

	// 读取版图文件
	void readLayout(std::ifstream& layout_file) {
		std::string line;
		int layer_id = -1;
		int polygon_id = -1;

		while (std::getline(layout_file, line)) {
			if (line != "" && line[0] != '(') { // 层名字
				layer_id++;
				polygon_id_range_in_layer.emplace_back(polygon_id + 1, 0); // 新建层, 已知起始多边形id
				if (layer_id != 0) {
					polygon_id_range_in_layer[layer_id - 1].second = polygon_id; // 已知上一层末尾多边形id
				}
				layer_name_to_id[line] = layer_id;
				layer_id_to_name[layer_id] = line;
			} 
			else { // 多边形
				polygon_id++;
				Polygon* poly = new Polygon();
				poly->id = polygon_id;
				poly->layer_id = layer_id;

				// 自行解析整型坐标值，避免istringstream
				fastParseCoordinates(line, poly->vertex);
				// 多边形的矩形包络框
				Rect poly_rect = GetRectofPolygon(poly);
				poly->rect = poly_rect;

				// 是否扩大版图
				layout.update(poly_rect);
				// 新增多边形
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
				iss.clear();          // 清除错误状态
				iss.str(line);        // 设置流为新行
				
				if (flag == "StartPos") { // 输入起点
					StartPos startpos;
					std::string layer_name;
					iss >> layer_name;
					startpos.first = layer_name_to_id[layer_name];
					iss>> c >> startpos.second.first >> c >> startpos.second.second >> c;
					start_pos.emplace_back(startpos);
				}
				else if (flag == "Via") { // 输入Via规则
					std::string first_layer, second_layer;
					iss >> first_layer;
					while (iss >> second_layer) {
						int fid = layer_name_to_id[first_layer];
						int sid = layer_name_to_id[second_layer];
						if (fid > sid) { // 保证小的层id在前
							std::swap(fid, sid);
						}
						via_rules.insert(Edge(fid, sid));

						first_layer = second_layer; // 确保二者是相邻两层
					}
				}
				else if(flag == "Gate"){ // 输入Gate规则
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

	// 获取多边形的矩形包络框
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

			// 使用条件赋值而不是if-else
			xmin = (x < xmin) ? x : xmin;
			xmax = (x > xmax) ? x : xmax;
			ymin = (y < ymin) ? y : ymin;
			ymax = (y > ymax) ? y : ymax;
		}

		return Rect(xmin, ymin, xmax, ymax);
	}
	
private:
	// 快速坐标解析函数，避免使用istringstream
	void fastParseCoordinates(const std::string& line, Vertexs& vertex) {
		vertex.reserve(10); // 预估每个多边形10个点
		const char* ptr = line.c_str();
		const char* end = ptr + line.length();

		while (ptr < end) {
			// 寻找下一个 '('
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

			// 添加点到多边形
			vertex.push_back(MPoint_2(x, y));

			// 寻找对应的 ')'
			while (ptr < end && *ptr != ')') ptr++;
			if (ptr < end) ptr++; // 跳过 ')'
		}
	}
};