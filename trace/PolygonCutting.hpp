#pragma once
#include "public.h"
#include "Input.hpp"
#include "QuadTree.hpp"
#include "Intersect.hpp"
#include "../ManhattanBooleanSetOperation/ManhattanBooleanSetOperation.h"

// 多边形切割类定义
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
		po_polygons.reserve(po_layer_range.second - po_layer_range.first + 1);
		for (int i = po_layer_range.first; i <= po_layer_range.second; ++i) {
			po_polygons.push_back(input.polygons[i]);
		}
		// 提取AA层多边形
		std::vector<Polygon*> aa_polygons;
		int aa_layer_id = input.gate_rule.second;
		Range& aa_layer_range = input.polygon_id_range_in_layer[aa_layer_id];
		aa_polygons.reserve(aa_layer_range.second - aa_layer_range.first + 1);
		for (int i = aa_layer_range.first; i <= aa_layer_range.second; ++i) {
			aa_polygons.push_back(input.polygons[i]);
		}

		// 合并PO层重叠多边形
		Merge_PO(po_polygons);

		// PO层多边形切割AA层多边形
		robin_hood::unordered_map<int, std::list<Edge>> po_cut_edges; // 各个po对应的切割边, 此时key使用的是po多边形数组下标, value为切割边列表, 切割边节点使用的是aa多边形数组下标
		Cut_AA(po_polygons, aa_polygons, po_cut_edges);

		// 由于数量变化和id被修改，故按序重建所有多边形id
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
				// 插入切割后的AA层多边形
				for (auto& p : aa_polygons) {
					p->id = new_id++;
					new_polygons.push_back(p);
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
		std::cout << "After Merge and Cutting Polygons" << std::endl;
		input.PrintLayoutInfo();

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

private:
	// 合并PO层重叠多边形
	void Merge_PO(std::vector<Polygon*>& po_polygons) {
		// 重排id, 与下标对应
		 for (int i = 0; i < po_polygons.size(); i++) {
			 po_polygons[i]->id = i;
		 }
		// 建立空间索引
		std::string name = input.layer_id_to_name[input.gate_rule.first];
		QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, "PO:" + name);
		quad_tree->CreatIndex(po_polygons);
		// 相交检测求边
		std::vector<Edge> edges = getEdgeofQuadTree_mergePO(quad_tree);
		// 并查集获取所有联通分量
		UnionFindSet unionfs(po_polygons.size());
		for (auto& e : edges) unionfs.join(e.first, e.second);
		std::vector<std::vector<int>> components = unionfs.getComponents();
		// 合并每个联通分量的多边形
		std::vector<Polygon*> po_merged_polygons;
		MBSO::Overlay overlay;
		for (auto& comp : components) {
			if (comp.size() == 1) {
				po_merged_polygons.push_back(po_polygons[comp[0]]);
			}
			else {
				//Polygon_set_2 merged_poly_set;
				//for (auto& idx : comp) {
				//	Polygon_2& cgal_poly = po_polygons[idx]->cgal_poly;
				//	if (merged_poly_set.is_empty()) {
				//		merged_poly_set.insert(cgal_poly);
				//	}
				//	else {
				//		merged_poly_set.join(cgal_poly);
				//	}
				//}
				//// 合并后的多边形转回自定义Polygon类
				//std::list<Polygon_with_holes_2> res;
				//merged_poly_set.polygons_with_holes(std::back_inserter(res));
				//assert(res.size() == 1 && "合并后多边形应为单一多边形");
				//// 假设没孔洞
				//if (res.front().has_holes()) {
				//	std::cout << "Warning: Merged polygon has holes!" << std::endl;
				//}
				//Polygon_2 merged_poly = res.front().outer_boundary();
				MBSO::myPolygonSet merged_poly_set;
				for (auto& idx : comp) {
					Polygon_2& cgal_poly = po_polygons[idx]->cgal_poly;
					MBSO::myPolygon mpoly;
					mpoly.reserve(cgal_poly.size());
					for (auto& p : cgal_poly) {
						mpoly.emplace_back(MBSO::MPoint_2(int(p.x()), int(p.y())));
					}
					reverse(mpoly.begin(), mpoly.end());
					MBSO::MPolygonSet mps1(merged_poly_set);
					MBSO::myPolygonSet mpoly_2;
					mpoly_2.push_back(mpoly);
					MBSO::MPolygonSet mps2(mpoly_2);
					MBSO::MPolygonSet res;
					overlay.solve(&mps1, &mps2, &res, MBSO::OP_TYPE::UNION);
					merged_poly_set = MBSO::getResult(&res);
					// merged_poly_set = MBSO::join(merged_poly_set, mpoly);
				}
				// 合并后的多边形转回自定义Polygon类
				assert(merged_poly_set.size() == 1 && "合并后多边形应为单一多边形");
				reverse(merged_poly_set[0].begin(), merged_poly_set[0].end());
				Polygon_2 merged_poly;
				for (auto& p : merged_poly_set[0]) {
					merged_poly.push_back(Point_2(p.getX(), p.getY()));
				}
				Polygon* new_poly = new Polygon();
				new_poly->layer_id = po_polygons[comp[0]]->layer_id; // 保持层id不变
				new_poly->cgal_poly = merged_poly;
				new_poly->rect = input.GetRectofPolygon(new_poly);
				po_merged_polygons.push_back(new_poly);
				// 释放旧多边形内存
				for (auto& idx : comp) {
					delete po_polygons[idx];
					po_polygons[idx] = nullptr;
				}
			}
		}
		// 更新po_polygons
		po_polygons = std::move(po_merged_polygons);
		// 释放四叉树内存
		delete quad_tree;
	}

	// PO层多边形切割AA层多边形
	void Cut_AA(std::vector<Polygon*>& po_polygons, std::vector<Polygon*>& aa_polygons, 
		robin_hood::unordered_map<int, std::list<Edge>>& po_cut_edges) 
	{	
		// PO和AA合并重排id, 与下标对应
		std::vector<Polygon*> poly_ptr;
		poly_ptr.reserve(po_polygons.size() + aa_polygons.size());
		for (int i = 0; i < po_polygons.size(); i++) {
			po_polygons[i]->id = i;
			poly_ptr.push_back(po_polygons[i]);
		}
		for (int i = 0; i < aa_polygons.size(); i++) {
			aa_polygons[i]->id = po_polygons.size() + i;
			poly_ptr.push_back(aa_polygons[i]);
		}
		// 两层合并建立空间索引
		std::string name = input.layer_id_to_name[input.gate_rule.first] +"-"+ input.layer_id_to_name[input.gate_rule.second];
		QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, "PO-AA:" + name);
		quad_tree->CreatIndex(poly_ptr);
		// 相交检测求边，要求边的两点属于不同层
		std::vector<Edge> edges = getEdgeofQuadTree_cutAA(quad_tree); 

		// 对每个AA多边形，记录其被哪些PO多边形切割
		robin_hood::unordered_map<int, std::vector<int>> aa_cut_by_po; // key为AA多边形此时id, value为切割它的PO多边形此时id
		for (auto& e : edges) {
			// 已保证边顺序e.first < e.second , 所以e.first为PO多边形, e.second为AA多边形
			int po_id = e.first;
			int aa_id = e.second;
			aa_cut_by_po[aa_id].push_back(po_id);
		}

		// 对每个AA多边形，执行切割（即使不被切割的也要遍历，因为要重新插入到aa_cut_polygons）
		std::vector<Polygon*> aa_cut_polygons; // 用于记录切割后所有的AA多边形
		MBSO::Overlay overlay;
		for (auto& aa_poly : aa_polygons) {
			int aa_id = aa_poly->id;
			auto it = aa_cut_by_po.find(aa_id);
			// 未被任何PO切割
			if (it == aa_cut_by_po.end()) {
				aa_cut_polygons.push_back(poly_ptr[aa_id]);
				continue;
			}
			// 若被PO切割
			std::vector<int>& cutting_po = it->second; // 切割它的PO多边形id列表
			// 被一个PO切割
			if (cutting_po.size() == 1) {
				// Polygon_set_2 cut_poly_set;
				// cut_poly_set.insert(poly_ptr[aa_id]->cgal_poly);
				// cut_poly_set.difference(poly_ptr[cutting_po[0]]->cgal_poly);
				// std::list<Polygon_with_holes_2> res;
				// cut_poly_set.polygons_with_holes(std::back_inserter(res));
				// assert(res.size() > 1 && "切割后多边形应不止一个");
				// // 切割后多边形转回自定义Polygon类
				// bool first = true;
				// for (auto it = res.begin(); it != res.end(); ++it) {
				// 	Polygon_2 new_aa_cgal_poly = it->outer_boundary();
				MBSO::myPolygonSet cut_poly_set;
				MBSO::myPolygon mpoly1;
				mpoly1.reserve(poly_ptr[aa_id]->cgal_poly.size());
				for (auto& p : poly_ptr[aa_id]->cgal_poly) {
					mpoly1.emplace_back(MBSO::MPoint_2(int(p.x()), int(p.y())));
				}
				reverse(mpoly1.begin(), mpoly1.end());
				cut_poly_set.push_back(mpoly1);
				MBSO::MPolygonSet mps1(cut_poly_set);

				MBSO::myPolygon mpoly2;
				mpoly2.reserve(poly_ptr[cutting_po[0]]->cgal_poly.size());
				for (auto& p : poly_ptr[cutting_po[0]]->cgal_poly) {
					mpoly2.emplace_back(MBSO::MPoint_2(int(p.x()), int(p.y())));
				}
				reverse(mpoly2.begin(), mpoly2.end());
				MBSO::myPolygonSet mpolys_2;
				mpolys_2.push_back(mpoly2);
				MBSO::MPolygonSet mps2(mpolys_2);

				MBSO::MPolygonSet res;
				overlay.solve(&mps1, &mps2, &res, MBSO::OP_TYPE::DIFF);
				cut_poly_set = MBSO::getResult(&res);
				assert(cut_poly_set.size() > 1 && "切割后多边形应不止一个");
				// 切割后多边形转回自定义Polygon类
				bool first = true;
				for (auto it = cut_poly_set.begin(); it != cut_poly_set.end(); ++it) {
					reverse(it->begin(), it->end());
					Polygon_2 new_aa_cgal_poly;
					for (auto& p : *it) {
						new_aa_cgal_poly.push_back(Point_2(p.getX(), p.getY()));
					}

					Polygon* new_poly = new Polygon();
					new_poly->layer_id = poly_ptr[aa_id]->layer_id; // 保持层id不变
					new_poly->cgal_poly = new_aa_cgal_poly;
					new_poly->rect = input.GetRectofPolygon(new_poly);
					aa_cut_polygons.push_back(new_poly);
					if (!first) {
						// 链式记录该PO的切割边(注意节点对应的是aa_cut_polygons数组的下标)
						po_cut_edges[cutting_po[0]].emplace_back(Edge(aa_cut_polygons.size() - 2, aa_cut_polygons.size() - 1));
					}
					first = false;
				}
				// 释放旧多边形内存
				delete poly_ptr[aa_id];
				poly_ptr[aa_id] = nullptr;
			}
			// 被多个PO切割
			else {
				// Polygon_set_2 cut_poly_set;
				// cut_poly_set.insert(poly_ptr[aa_id]->cgal_poly);
				// for (auto& po_id : cutting_po) { // 逐个求差集
				// 	cut_poly_set.difference(poly_ptr[po_id]->cgal_poly);
				// }
				// std::list<Polygon_with_holes_2> res;
				// cut_poly_set.polygons_with_holes(std::back_inserter(res));
				// assert(res.size() > 1 && "切割后多边形应不止一个");
				// // 切割后多边形转回自定义Polygon类
				// robin_hood::unordered_map<int, std::vector<int>> po_cut_nodes; // 记录该AA被切割后,各PO连接的AA多边形节点
				// for (auto it = res.begin(); it != res.end(); ++it) {
				// 	Polygon_2 new_aa_cgal_poly = it->outer_boundary();
				MBSO::myPolygonSet cut_poly_set;
				MBSO::myPolygon mpoly1;
				mpoly1.reserve(poly_ptr[aa_id]->cgal_poly.size());
				for (auto& p : poly_ptr[aa_id]->cgal_poly) {
					mpoly1.emplace_back(MBSO::MPoint_2(int(p.x()), int(p.y())));
				}
				reverse(mpoly1.begin(), mpoly1.end());
				cut_poly_set.push_back(mpoly1);
				MBSO::MPolygonSet mps1(cut_poly_set);

				MBSO::myPolygonSet mpolys_2;
				for (auto& po_id : cutting_po) { // 逐个求差集
					MBSO::myPolygon mpoly2;
					mpoly2.reserve(poly_ptr[po_id]->cgal_poly.size());
					for (auto& p : poly_ptr[po_id]->cgal_poly) {
						mpoly2.emplace_back(MBSO::MPoint_2(int(p.x()), int(p.y())));
					}
					reverse(mpoly2.begin(), mpoly2.end());
					mpolys_2.push_back(mpoly2);
				}
				MBSO::MPolygonSet mps2(mpolys_2);

				MBSO::MPolygonSet res;
				overlay.solve(&mps1, &mps2, &res, MBSO::OP_TYPE::DIFF);
				cut_poly_set = MBSO::getResult(&res);
				assert(cut_poly_set.size() > 1 && "切割后多边形应不止一个");
				// 切割后多边形转回自定义Polygon类
				robin_hood::unordered_map<int, std::vector<int>> po_cut_nodes; // 记录该AA被切割后,各PO连接的AA多边形节点
				for (auto it = cut_poly_set.begin(); it != cut_poly_set.end(); ++it) {
					reverse(it->begin(), it->end());
					Polygon_2 new_aa_cgal_poly;
					for (auto& p : *it) {
						new_aa_cgal_poly.push_back(Point_2(p.getX(), p.getY()));
					}
					Polygon* new_poly = new Polygon();
					new_poly->layer_id = poly_ptr[aa_id]->layer_id; // 保持层id不变
					new_poly->cgal_poly = new_aa_cgal_poly;
					new_poly->rect = input.GetRectofPolygon(new_poly);
					aa_cut_polygons.push_back(new_poly);

					for (auto& po_id : cutting_po) { // 检查该新多边形属于哪个PO切割的
						if (ManhattanCompleteIntersectionDetector::manhattanPolygonsIntersect(new_poly, poly_ptr[po_id])){
							po_cut_nodes[po_id].push_back(aa_cut_polygons.size() - 1);
						}
					}
				}
				// AA被多个PO切割, 故遍历所有切割它的PO, 为每个PO链式记录他的切割边
				for (auto& item : po_cut_nodes) {
					int po_id = item.first;
					std::vector<int>& nodes = item.second; // 该PO连接的AA多边形节点
					// 链式记录该PO的切割边(注意节点对应的是aa_cut_polygons数组的下标)
					for (int i = 1; i < nodes.size(); i++) {
						po_cut_edges[po_id].emplace_back(Edge(nodes[i - 1], nodes[i]));
					}
				}
				// 释放旧多边形内存
				delete poly_ptr[aa_id];
				poly_ptr[aa_id] = nullptr;
			}
		}
		// 更新aa_polygons
		aa_polygons = std::move(aa_cut_polygons);
		// 释放四叉树内存
		delete quad_tree;
	}

	// 执行给定索引的多边形相交检测，获取其边的集合
	std::vector<std::pair<int, int>> getEdgeofQuadTree_mergePO(QuadTree* qtree) {
		std::vector<std::pair<int, int>> edges;
		edges.reserve(500000);
		// 收集空间索引
		std::vector<std::vector<Polygon*>> leafData;
		std::cout << "Handling QuadTree : " << qtree->_name << std::endl;
		qtree->GetAllLeafData(leafData);

		// 根据空间索引（小格子内），对其内多边形执行相交检测
		for (auto& lfd : leafData) {
			if (lfd.size() < 2) continue;
			for (int i = 0; i < (int)lfd.size(); i++) {
				Polygon* a = lfd[i];
				for (int j = i + 1; j < (int)lfd.size(); j++) {
					Polygon* b = lfd[j];
					// 使用曼哈顿多边形相交检测
					if (ManhattanCompleteIntersectionDetector::manhattanPolygonsIntersect(a, b)) {
						edges.emplace_back(a->id, b->id);
					}
				}
			}
		}
		// 规范边
		for (auto& edge : edges) {
			if (edge.first > edge.second) {
				std::swap(edge.first, edge.second);
			}
		}
		// 排序并去重
		std::sort(edges.begin(), edges.end());
		edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
		return edges;
	}

	// 执行给定索引的多边形相交检测，获取其边的集合
	std::vector<std::pair<int, int>> getEdgeofQuadTree_cutAA(QuadTree* qtree) {
		std::vector<std::pair<int, int>> edges;
		edges.reserve(500000);
		// 收集空间索引
		std::vector<std::vector<Polygon*>> leafData;
		std::cout << "Handling QuadTree : " << qtree->_name << std::endl;
		qtree->GetAllLeafData(leafData);

		// 根据空间索引（小格子内），对其内多边形执行相交检测
		for (auto& lfd : leafData) {
			if (lfd.size() < 2) continue;
			for (int i = 0; i < (int)lfd.size(); i++) {
				Polygon* a = lfd[i];
				for (int j = i + 1; j < (int)lfd.size(); j++) {
					Polygon* b = lfd[j];
					if (a->layer_id == b->layer_id) continue; // 同层不检测
					// 使用曼哈顿多边形相交检测
					if (ManhattanCompleteIntersectionDetector::manhattanPolygonsIntersect(a, b)) {
						edges.emplace_back(a->id, b->id);
					}
				}
			}
		}
		// 规范边
		for (auto& edge : edges) {
			if (edge.first > edge.second) {
				std::swap(edge.first, edge.second);
			}
		}
		// 排序并去重
		std::sort(edges.begin(), edges.end());
		edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
		return edges;
	}
};