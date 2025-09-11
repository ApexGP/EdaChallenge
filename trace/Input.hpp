#pragma once
#include "public.h"
#include <fstream>
#include <sstream>
#include <iostream>

class Input {
public:
	std::unordered_map<std::string, int> layer_name_to_id;  // 层名转层id, 0-index, 代码内使用id指代层
	std::unordered_map<int, std::string> layer_id_to_name;  // 层id转层名, 方便输出
	int total_polygon;										// 多边形总数
	std::vector<Polygon> polygons;							// 所有的多边形列表, 下标对应其多边形id, 0-index
	std::vector<Range> polygon_id_range_in_layer;			// 每层的多边形的id范围, 双闭区间, 下标对应其层id
	std::set<Edge> via_rules;								// Via规则集合，表示哪两层连通, 通过输入保证层id小的在前，结合set确保不重复
	bool has_gate_rule;										// 是否存在Gate规则
	Edge gate_rule;											// 若存在则表示Gate规则, gate_rule.first为Poly层，gate_rule.second为AA层
	std::vector<StartPos> start_pos;						// 起点位置, 至少一个, 最多两个, 按输入顺序


	// 给定文件路径，获取输入
	Input(std::string layout_path, std::string rule_path) {
		polygons.reserve(1000000); // 预分配一些空间

		std::ifstream layout_file(layout_path); // 创建并打开文件
		assert(layout_file.is_open() && "无法打开layout文件");
		readLayout(layout_file);
		layout_file.close();

		std::ifstream rule_file(rule_path);
		assert(rule_file.is_open() && "无法打开rule文件");
		readRule(rule_file);
		rule_file.close();
	}

private:
	void readLayout(std::ifstream& layout_file) {
		std::istringstream iss;
		std::string line;
		int layer_id = -1;
		int polygon_id = -1;

		while (std::getline(layout_file, line)) {
			if (line != "" && line[0] != '(') { // 层名字
				layer_id++;
				polygon_id_range_in_layer.push_back(Range(polygon_id + 1, 0)); // 新建层, 已知起始多边形id
				if (layer_id != 0) {
					polygon_id_range_in_layer[layer_id - 1].second = polygon_id; // 已知上一层末尾多边形id
				}
				layer_name_to_id[line] = layer_id;
				layer_id_to_name[layer_id] = line;
			} 
			else { // 多边形
				polygon_id++;
				Polygon poly;
				poly.id = polygon_id;
				poly.layer_id = layer_id;

				iss.clear();          // 清除错误状态
				iss.str(line);        // 设置流为新行

				// 解析逻辑
				int x, y;
				char c;
				while (iss >> c) {
					if (c == '(') { // 解析坐标
						iss >> x >> c >> y;
						poly.vetex.push_back(Point(x, y));
					}
				}
				// 新增多边形
				polygons.emplace_back(std::move(poly));
			}
		}
		polygon_id_range_in_layer[layer_id].second = polygon_id; // 最后一层末尾多边形id
		total_polygon = polygon_id + 1;
	
		// check print
		std::cout << "【Layout】" << std::endl;
		std::cout << "total polygon num:" << total_polygon << std::endl;
		std::cout << "total layer num:" << polygon_id_range_in_layer.size() << std::endl;
		for (auto& name_id : layer_name_to_id) {
			std::cout << "layer name:" << name_id.first << " id:" << name_id.second
				<< " polygon num:" << polygon_id_range_in_layer[name_id.second].second - polygon_id_range_in_layer[name_id.second].first + 1
				<< " polygon id range:[" << polygon_id_range_in_layer[name_id.second].first << "," << polygon_id_range_in_layer[name_id.second].second << "]" << std::endl;
		}
	}

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
					start_pos.push_back(startpos);
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
				else { // 输入Gate规则
					has_gate_rule = true;
					std::string first_layer, second_layer;
					iss >> first_layer >> second_layer;
					int fid = layer_name_to_id[first_layer];
					int sid = layer_name_to_id[second_layer];
					gate_rule = Edge(fid, sid);
				}
			}
		}
		
		// check print
		std::cout << "【Rule】" << std::endl;
		for (auto& st : start_pos)
			std::cout << "StartPos:" << layer_id_to_name[st.first] << " " << st.second.first <<" " << st.second.second << std::endl;
		for (auto& vi : via_rules)
			std::cout << "Via:" << layer_id_to_name[vi.first] << " " << layer_id_to_name[vi.second] << std::endl;
		if(has_gate_rule)
			std::cout << "Gate:" << layer_id_to_name[gate_rule.first] << " " << layer_id_to_name[gate_rule.second] << std::endl;
		else 
			std::cout << "no Gate" << std::endl;
	}
};