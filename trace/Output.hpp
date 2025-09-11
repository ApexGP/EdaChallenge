#pragma once
#include "public.h"
#include "Input.hpp"

class Output {
public:
	// 给定输入数据、输出文件路径、连通分量，解析输出链路追踪结果
	Output(Input& _input, std::string res_path, std::vector<int>& _component):input(_input), component(_component){
		std::ofstream res_file(res_path); // 创建并打开文件
		assert(res_file.is_open() && "无法打开res文件");
		printResult(res_file);
		res_file.close();
	}

private:
	Input& input;
	std::vector<int>& component;

	void printResult(std::ofstream& res_file) {
		int layer_num = (int)input.polygon_id_range_in_layer.size();
		std::vector<std::vector<int>> layer_polygons(layer_num);  // 结果中每层的多边形id的列表
		
		// 遍历结果, 按层分类
		auto& polygons = input.polygons;
		for (auto& poly_id : component) {
			int layer_id = polygons[poly_id].layer_id;
			layer_polygons[layer_id].push_back(poly_id);
		}

		// 按层顺序输出
		for (int i = 0; i < (int)layer_polygons.size(); ++i){
			if (layer_polygons[i].size() != 0) {
				//输出层名
				res_file << input.layer_id_to_name[i] << std::endl;
				//输出每个多边形
				for (auto& poly_id : layer_polygons[i]) {
					Polygon& p = polygons[poly_id];
					//输出每个顶点
					for (int j = 0; j < (int)p.vetex.size(); ++j) {
						res_file << "(" << p.vetex[j].first << "," << p.vetex[j].second << ")";
						if (j != p.vetex.size() - 1) {
							res_file << ",";
						}
					}
					res_file << std::endl;
				}
			}
		}
	}
};