#pragma once
#include "public.h"
#include "Input.hpp"

class Output {
public:
	// 给定输入数据、输出文件路径、连通分量，解析输出链路追踪结果
	Output(Input& _input, std::string res_path, std::vector<int>& _component):input(_input), component(_component){
		output_poly_buffer.reserve(400); // 预分配缓冲区, 按每个顶点约20字符, 20个顶点预估
		std::ofstream res_file(res_path); // 创建并打开文件
		assert(res_file.is_open() && "无法打开res文件");
		printResult(res_file);
		res_file.close();
	}

private:
	Input& input;
	std::vector<int>& component;

	// 中间数据结构
	std::string output_poly_buffer; // 用于存储单个多边形的输出字符串
	char num_buf[12]; 				// int转str缓冲区, 足够存储-2147483648到2147483647

	void printResult(std::ofstream& res_file) {
		int layer_num = (int)input.polygon_id_range_in_layer.size();
		std::vector<std::vector<int>> layer_polygons(layer_num);  // 结果中每层的多边形id的列表
		
		// 遍历结果, 按层分类
		auto& polygons = input.polygons;
		for (auto& poly_id : component) {
			int layer_id = polygons[poly_id].layer_id;
			layer_polygons[layer_id].emplace_back(poly_id);
		}

		// 按层顺序输出
		for (int i = 0; i < (int)layer_polygons.size(); ++i){
			if (layer_polygons[i].size() != 0) {
				//输出层名
				res_file << input.layer_id_to_name[i] << std::endl;
				//输出每个多边形
				for (auto& poly_id : layer_polygons[i]) {
					OutputPolygon(res_file, polygons[poly_id]);
				}
			}
		}
	}

	// 高效输出一个多边形
	void OutputPolygon(std::ofstream &res_file, Polygon &p){
		output_poly_buffer.clear();
		std::string &buffer = output_poly_buffer;

		// 使用快速整数转换并构建缓冲区
		for (size_t j = 0; j < p.cgal_poly.size(); ++j){
			buffer += '(';
			AppendInt(buffer, static_cast<int>(p.cgal_poly[j].x()));
			buffer += ',';
			AppendInt(buffer, static_cast<int>(p.cgal_poly[j].y()));
			buffer += ')';

			if (j != p.cgal_poly.size() - 1){
				buffer += ',';
			}
		}

		// 一次性输出
		buffer += '\n';
		res_file << buffer;
	}

	// 优化的整数追加函数
	void AppendInt(std::string &str, int x){
		if (x == 0){
			str += '0';
			return;
		}

		// 处理负数
		if (x < 0){
			str += '-';
			// 处理INT_MIN特殊情况
    		if (x == INT_MIN) {
        		str += "2147483648";
        		return;
    		}
			x = -x;
		}

		int idx = 0;
		// 转换数字
		while (x > 0){
			num_buf[idx++] = '0' + (x % 10);
			x /= 10;
		}

		// 反向追加到字符串
		while (idx > 0){
			str += num_buf[--idx];
		}
	}
};