#pragma once
#include "public.h"
#include "Input.hpp"

// 空间索引类定义
class PolygonCutting
{
private:
	Input& input;

public:
	PolygonCutting(Input& _input) :input(_input) {
		assert(input.has_gate_rule && "请保证存在Gate规则");
	}
	
	robin_hood::unordered_map<int, std::list<Edge>> MergePOAndCutAA() {
		// 提取PO层多边形
		std::vector<Polygon*> po_polygons;
		int po_layer_id = input.gate_rule.first;
		Range& po_layer_range = input.polygon_id_range_in_layer[po_layer_id];
		for (int i = po_layer_range.first; i <= po_layer_range.second; ++i) {
			po_polygons.push_back(input.polygons[i]);
		}
		// 提取AA层多边形
		std::vector<Polygon*> aa_polygons;
		int aa_layer_id = input.gate_rule.second;
		Range& aa_layer_range = input.polygon_id_range_in_layer[aa_layer_id];
		for (int i = aa_layer_range.first; i <= aa_layer_range.second; ++i) {
			aa_polygons.push_back(input.polygons[i]);
		}

		// 合并PO层重叠多边形
		Merge_PO(po_polygons);

		// PO层多边形切割AA层多边形
		robin_hood::unordered_map<int, std::list<Edge>> po_cut_edges; // 各个po对应的切割边, key使用的是po多边形数组下标, value为切割边列表, 边节点使用的是aa多边形数组下标
		Cut_AA(po_polygons, aa_polygons, po_cut_edges);

		// 由于数量变化，故重建多边形id
		int new_id = 0;
		std::vector<Polygon*> new_polygons;
		for (int i = 0; i < input.polygons.size(); i++) {
			if (i == po_layer_range.first) {
				// 插入合并后的PO层多边形
				for (auto& p : po_polygons) {
					p->id = new_id++;
					new_polygons.push_back(p);
				}
				i = po_layer_range.second; // 跳过旧的PO层多边形
			}
			else if (i == aa_layer_range.first) {
				// 插入未变化的AA层多边形
				for (int j = aa_layer_range.first; j <= aa_layer_range.second; j++) {
					input.polygons[j]->id = new_id++;
					new_polygons.push_back(input.polygons[j]);
				}
				i = aa_layer_range.second; // 跳过旧的AA层多边形
			}
			else {
				// 插入其他层未变化的多边形
				input.polygons[i]->id = new_id++;
				new_polygons.push_back(input.polygons[i]);
			}
		}
		input.polygons = std::move(new_polygons);
		input.total_polygon = new_id;

		// 更新各层多边形id范围
		int begin = 0;
		for (int i = 0; i < input.polygon_id_range_in_layer.size(); i++) {
			if (i == po_layer_id) {
				int layer_size = (int)po_polygons.size();
				input.polygon_id_range_in_layer[i] = Range(begin, begin + layer_size - 1);
				begin += layer_size;
			}
			else if (i == aa_layer_id) {
				int layer_size = (int)aa_polygons.size();
				input.polygon_id_range_in_layer[i] = Range(begin, begin + layer_size - 1);
				begin += layer_size;
			}
			else {
				int layer_size = input.polygon_id_range_in_layer[i].second - input.polygon_id_range_in_layer[i].first + 1;
				input.polygon_id_range_in_layer[i] = Range(begin, begin + layer_size - 1);
				begin += layer_size;
			}
		}

		// 新的id已确定，可更新po_cut_edges为使用对应的多边形id表示
		robin_hood::unordered_map<int, std::list<Edge>> updated_po_cut_edges;
		for (auto& item : po_cut_edges) {
			int po_index = item.first; // 原为数组下标
			int po_id = po_polygons[po_index]->id; // 获取对应的多边形id
			for (auto& edge : item.second) {
				int aa_id1 = aa_polygons[edge.first]->id; // 获取对应的多边形id
				int aa_id2 = aa_polygons[edge.second]->id; 
				updated_po_cut_edges[po_id].emplace_back(Edge(aa_id1, aa_id2)); // 使用新的多边形id
			}
		}

		// 返回结果
		return updated_po_cut_edges;
	}

	// 合并PO层重叠多边形
	void Merge_PO(std::vector<Polygon*>& po_polygons) {

	}

	// PO层多边形切割AA层多边形
	void Cut_AA(std::vector<Polygon*>& po_polygons, std::vector<Polygon*>& aa_polygons, 
		robin_hood::unordered_map<int, std::list<Edge>>& po_cut_edges) 
	{

	}
};