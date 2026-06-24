#pragma once
#include "public.h"
#include "Input.hpp"
#include "QuadTree.hpp"
#include "Intersect.hpp"
#include "ManhattanIntersectDetector.hpp"
#include "./ManhattanBooleanSetOperation/solve/MBSOCore.h"

static thread_local MBSO::MBSOCore mbsoCore; // 曼哈顿多边形布尔运算核心类

// 多边形切割类定义
class PolygonCutting
{
private:
	Input& input;

public:
	PolygonCutting(Input& _input) :input(_input) {
		assert(input.has_gate_rule && "there is not exist gate rule! no need");
	}

	robin_hood::unordered_map<int, std::vector<Edge>> MergePOAndCutAA() {
		// 提取PO层多边形
		int po_layer_id = input.gate_rule.first;
		Range& po_layer_range = input.polygon_id_range_in_layer[po_layer_id];
		std::vector<Polygon> po_polygons;
		po_polygons.reserve(po_layer_range.second - po_layer_range.first + 1);
		for (int i = po_layer_range.first; i <= po_layer_range.second; ++i){
			po_polygons.emplace_back(std::move(input.polygons[i]));
		}

		// 提取AA层多边形
		int aa_layer_id = input.gate_rule.second;
		Range& aa_layer_range = input.polygon_id_range_in_layer[aa_layer_id];
		std::vector<Polygon> aa_polygons;
		aa_polygons.reserve(aa_layer_range.second - aa_layer_range.first + 1);
		for (int i = aa_layer_range.first; i <= aa_layer_range.second; ++i){
			aa_polygons.emplace_back(std::move(input.polygons[i]));
		}

		// 合并PO层重叠多边形
		Merge_PO(po_polygons);

		// PO层多边形切割AA层多边形
		robin_hood::unordered_map<int, std::vector<Edge>> po_cut_edges; // 各个po对应的切割边, 此时key使用的是po多边形数组下标, value为切割边列表, 切割边节点使用的是aa多边形数组下标
		po_cut_edges.reserve(po_polygons.size());
		Cut_AA(po_polygons, aa_polygons, po_cut_edges);

		// 由于PO层和AA层数量变化和id被修改，故按序重建他们的id，注意我们输入时保证了PO和AA分别是倒二和倒一层，他们的id也是排在末尾的，所以不会影响其他层
		int po_start_id = input.polygon_id_range_in_layer[po_layer_id].first; // po起始id
		int new_total_polygon_size = po_start_id + po_polygons.size() + aa_polygons.size();
		input.total_polygon = new_total_polygon_size;
		input.polygons.resize(new_total_polygon_size);

		// 插入合并后的PO层多边形,并更新信息
		for (int i = 0; i < po_polygons.size(); i++) {
			input.polygons[po_start_id + i] = std::move(po_polygons[i]);
			input.polygons[po_start_id + i].id = po_start_id + i;
		}
		input.polygon_id_range_in_layer[po_layer_id] = Range(po_start_id, po_start_id + po_polygons.size() - 1);

		int aa_start_id = input.polygon_id_range_in_layer[po_layer_id].second + 1; // aa起始id
		// 插入合并后的AA层多边形,并更新信息
		for (int i = 0; i < aa_polygons.size(); i++) {
			input.polygons[aa_start_id + i] = std::move(aa_polygons[i]);
			input.polygons[aa_start_id + i].id = aa_start_id + i;
		}
		input.polygon_id_range_in_layer[aa_layer_id] = Range(aa_start_id, aa_start_id + aa_polygons.size() - 1);

		INFO_MSG( "After Merge and Cutting Polygons" )
		INFO_INSTR(input.PrintLayoutInfo();)

		// 新的id已确定，可更新po_cut_edges为使用对应的多边形id表示
		robin_hood::unordered_map<int, std::vector<Edge>> updated_po_cut_edges;
		updated_po_cut_edges.reserve(po_cut_edges.size());  // 预分配

		for (auto& [po_index, edges] : po_cut_edges) {
			// 直接在新映射表中使用正确ID，避免中间容器
			auto& new_edges = updated_po_cut_edges[po_start_id + po_index];
			new_edges.reserve(edges.size());  // 预分配每条边的列表

			for (auto& edge : edges) {  // 使用新的多边形id
				new_edges.emplace_back(
					aa_start_id + edge.first,
					aa_start_id + edge.second
				);
			}
		}

		// 返回结果
		return updated_po_cut_edges;
	}

private:
	// 合并PO层重叠多边形
	void Merge_PO(std::vector<Polygon>& po_polygons) {
		std::vector<Polygon*> po_polygons_ptr;
		po_polygons_ptr.reserve(po_polygons.size());
		// 重排id, 与下标对应，并提取指针
		for (int i = 0; i < po_polygons.size(); i++) {
			 po_polygons[i].id = i;
			 po_polygons_ptr.push_back(&po_polygons[i]);
		}
		// 建立空间索引
		std::string name = input.layer_id_to_name[input.gate_rule.first];
		QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, "PO:" + name);
		quad_tree->CreatIndex(std::move(po_polygons_ptr));
		// 相交检测求边
		std::vector<Edge> edges = getEdgeofQuadTree_mergePO(quad_tree);
		// 并查集获取所有联通分量
		UnionFindSet unionfs(po_polygons.size());
		for (auto& e : edges) unionfs.join(e.first, e.second);
		std::vector<std::vector<int>> components = unionfs.getComponents();
		// 合并每个联通分量的多边形
		std::vector<Polygon> po_merged_polygons;
		po_merged_polygons.resize(components.size());
		for (int i = 0; i < components.size(); ++i) {
			auto& comp = components[i];
			if (comp.size() == 1) {
				po_merged_polygons[i] = std::move(po_polygons[comp[0]]);
			}
			else {
				Vertexs& po_vertex1 = po_polygons[comp[0]].vertex;
				mbsoCore.setResultMPS(po_vertex1); // 设置第一个多边形
				// 序列式求并集
				for (int i = 1; i < comp.size(); ++i) {
					int idx = comp[i];
					Vertexs& po_vertex2 = po_polygons[idx].vertex;
					mbsoCore.join(po_vertex2);
				}

				// 合并后的多边形转回自定义Polygon类
				std::vector<std::vector<MPoint_2>> merged_poly_set = mbsoCore.getResult();
				assert(merged_poly_set.size() == 1 && "合并后多边形应为单一多边形");
				// 构建多边形
				Polygon& new_poly = po_merged_polygons[i];
				new_poly.layer_id = po_polygons[comp[0]].layer_id; // 保持层id不变
				new_poly.vertex = std::move(merged_poly_set[0]);
				new_poly.rect = input.GetRectofPolygon(new_poly.vertex);
			}
		}
		// 更新po_polygons
		po_polygons = std::move(po_merged_polygons);
		// 释放四叉树内存
		delete quad_tree;
	}

	// PO层多边形切割AA层多边形
	void Cut_AA(std::vector<Polygon>& po_polygons, std::vector<Polygon>& aa_polygons,
		robin_hood::unordered_map<int, std::vector<Edge>>& po_cut_edges)
	{
		// PO和AA合并重排id, 与下标对应
		std::vector<Polygon*> poly_ptr;
		poly_ptr.reserve(po_polygons.size() + aa_polygons.size());
		for (int i = 0; i < po_polygons.size(); i++) {
			po_polygons[i].id = i;
			poly_ptr.push_back(&po_polygons[i]);
		}
		for (int i = 0; i < aa_polygons.size(); i++) {
			aa_polygons[i].id = po_polygons.size() + i;
			poly_ptr.push_back(&aa_polygons[i]);
		}
		// 两层合并建立空间索引
		std::string name = input.layer_id_to_name[input.gate_rule.first] +"-"+ input.layer_id_to_name[input.gate_rule.second];
		QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, "PO-AA:" + name);
		quad_tree->CreatIndex(poly_ptr);
		// 相交检测求边，要求边的两点属于不同层
		std::vector<Edge> edges = getEdgeofQuadTree_cutAA(quad_tree);

		// 对每个AA多边形，记录其被哪些PO多边形切割
		robin_hood::unordered_map<int, std::vector<int>> aa_cut_by_po; // key为AA多边形此时id, value为切割它的PO多边形此时id
		aa_cut_by_po.reserve(aa_polygons.size());
		for (auto& e : edges) {
			// 已保证边顺序e.first < e.second , 所以e.first为PO多边形, e.second为AA多边形
			int po_id = e.first;
			int aa_id = e.second;
			aa_cut_by_po[aa_id].push_back(po_id);
		}

		// 对每个AA多边形，执行切割（即使不被切割的也要遍历，因为要重新插入到aa_cut_polygons）
		std::vector<Polygon> aa_cut_polygons; // 用于记录切割后所有的AA多边形
		aa_cut_polygons.reserve(aa_polygons.size() * 7); // 预估平均一个被切成7个
		for (auto& aa_poly : aa_polygons) {
			int aa_id = aa_poly.id;
			auto it = aa_cut_by_po.find(aa_id);
			// 未被任何PO切割
			if (it == aa_cut_by_po.end()) {
				aa_cut_polygons.emplace_back(std::move(*poly_ptr[aa_id]));
				continue;
			}
			// 若被PO切割
			std::vector<int>& cutting_po = it->second; // 切割它的PO多边形id列表
			// 被一个PO切割
			if (cutting_po.size() == 1) {
				// 设置 AA 多边形
				Vertexs& aa_vertex = poly_ptr[aa_id]->vertex;
				mbsoCore.setResultMPS(aa_vertex);
				// 求差集切割
				Vertexs& po_vertex = poly_ptr[cutting_po[0]]->vertex;
				mbsoCore.difference(po_vertex);
				// 获取结果
				std::vector<std::vector<MBSO::MPoint_2>> cut_poly_set = mbsoCore.getResult();
				assert(cut_poly_set.size() > 1 && "切割后多边形应不止一个");

				// 切割后多边形转回自定义Polygon类
				bool first = true;
				for (auto it = cut_poly_set.begin(); it != cut_poly_set.end(); ++it) {
					// reverse(it->begin(), it->end());
					Polygon new_poly;
					new_poly.layer_id = poly_ptr[aa_id]->layer_id; // 保持层id不变
					new_poly.vertex = std::move(*it);
					new_poly.rect = input.GetRectofPolygon(new_poly.vertex);
					aa_cut_polygons.emplace_back(std::move(new_poly));
					if (!first) {
						// 链式记录该PO的切割边(注意节点对应的是aa_cut_polygons数组的下标)
						po_cut_edges[cutting_po[0]].emplace_back(aa_cut_polygons.size() - 2, aa_cut_polygons.size() - 1);
					}
					first = false;
				}
			}
			// 被多个PO切割
			else {
				// 设置 AA 多边形
				Vertexs& aa_vertex = poly_ptr[aa_id]->vertex;
				mbsoCore.setResultMPS(aa_vertex);

				// 构造多个po的多边形集合
				std::vector<std::vector<MPoint_2>> mpolys_2;
				mpolys_2.reserve(cutting_po.size());
				for (auto& po_id : cutting_po) {
					Vertexs& po_vertex = poly_ptr[po_id]->vertex;
					mpolys_2.push_back(po_vertex);
				}
				robin_hood::unordered_map<int, std::vector<int>> po_cut_nodes; // 记录该AA被切割后,各PO(数组下标)连接的AA多边形(数组下标)
				// 一次性求差集切割
				mbsoCore.difference(mpolys_2, po_cut_nodes);
				// 获取结果
				std::vector<std::vector<MBSO::MPoint_2>> cut_poly_set = mbsoCore.getResult();
				assert(cut_poly_set.size() > 1 && "切割后多边形应不止一个");

				// 切割后多边形转回自定义Polygon类
				for (auto it = cut_poly_set.begin(); it != cut_poly_set.end(); ++it) {
					// reverse(it->begin(), it->end());
					Polygon new_poly;
					new_poly.layer_id = poly_ptr[aa_id]->layer_id; // 保持层id不变
					new_poly.vertex = std::move(*it);
					new_poly.rect = input.GetRectofPolygon(new_poly.vertex);
					aa_cut_polygons.emplace_back(std::move(new_poly));

					//for (auto& po_id : cutting_po) { // 检查该新多边形属于哪个PO切割的
					//	if (ManhattanIntersectDetector::manhattanPolygonsIntersect(new_poly, poly_ptr[po_id])){
					//		po_cut_nodes[po_id].push_back(aa_cut_polygons.size() - 1);
					//	}
					//}
				}

				// AA被多个PO切割, 故遍历所有切割它的PO, 为每个PO链式记录他的切割边
				for (auto& item : po_cut_nodes) {
					int po_id = cutting_po[item.first]; // 下标映射到实际id
					std::vector<int>& nodes = item.second; // 该PO连接的AA多边形节点
					// 链式记录该PO的切割边(注意最终节点对应的是aa_cut_polygons数组的下标)
					int offset = aa_cut_polygons.size() - cut_poly_set.size();
					for (int i = 1; i < nodes.size(); i++) {
						po_cut_edges[po_id].emplace_back(offset + nodes[i - 1], offset + nodes[i]);
					}
				}
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
		std::vector<QuadTreeNode*> leafNode;
		INFO_MSG( "Handling QuadTree : " << qtree->_name )
		qtree->GetAllLeafNode(leafNode);

		// 根据空间索引（小格子内），对其内多边形执行相交检测
		for (auto& lfNode : leafNode) {
			auto& lfd = lfNode->_datas;
			if (lfd.size() < 2) continue;
			for (int i = 0; i < (int)lfd.size(); i++) {
				Polygon* a = lfd[i];
				for (int j = i + 1; j < (int)lfd.size(); j++) {
					Polygon* b = lfd[j];
					// 使用曼哈顿多边形相交检测
					if (ManhattanIntersectDetector::manhattanPolygonsIntersect(a, b)) {
						// 规范边（确保 first < second）
    	                int min_id = std::min(a->id, b->id);
    	                int max_id = std::max(a->id, b->id);
    	                edges.emplace_back(min_id, max_id);
					}
				}
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
		std::vector<QuadTreeNode*> leafNode;
		INFO_MSG( "Handling QuadTree : " << qtree->_name )
		qtree->GetAllLeafNode(leafNode);

		// 根据空间索引（小格子内），对其内多边形执行相交检测
		for (auto& lfNode : leafNode) {
			auto& lfd = lfNode->_datas;
			if (lfd.size() < 2) continue;
			for (int i = 0; i < (int)lfd.size(); i++) {
				Polygon* a = lfd[i];
				for (int j = i + 1; j < (int)lfd.size(); j++) {
					Polygon* b = lfd[j];
					if (a->layer_id == b->layer_id) continue; // 同层不检测
					// 使用曼哈顿多边形相交检测
					if (ManhattanIntersectDetector::manhattanPolygonsIntersect(a, b)) {
						// 规范边（确保 first < second）
    	                int min_id = std::min(a->id, b->id);
    	                int max_id = std::max(a->id, b->id);
    	                edges.emplace_back(min_id, max_id);
					}
				}
			}
		}
		// 排序并去重
		std::sort(edges.begin(), edges.end());
		edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
		return edges;
	}

#pragma region parallel_version
public:
	robin_hood::unordered_map<int, std::vector<Edge>> MergePOAndCutAAParallel(int thread_count) {
		// 提取PO层多边形
		int po_layer_id = input.gate_rule.first;
		Range& po_layer_range = input.polygon_id_range_in_layer[po_layer_id];
		std::vector<Polygon> po_polygons;
		po_polygons.reserve(po_layer_range.second - po_layer_range.first + 1);
		for (int i = po_layer_range.first; i <= po_layer_range.second; ++i){
			po_polygons.emplace_back(std::move(input.polygons[i]));
		}

		// 提取AA层多边形
		int aa_layer_id = input.gate_rule.second;
		Range& aa_layer_range = input.polygon_id_range_in_layer[aa_layer_id];
		std::vector<Polygon> aa_polygons;
		aa_polygons.reserve(aa_layer_range.second - aa_layer_range.first + 1);
		for (int i = aa_layer_range.first; i <= aa_layer_range.second; ++i){
			aa_polygons.emplace_back(std::move(input.polygons[i]));
		}

		// 合并PO层重叠多边形
		Merge_PO_Parallel(po_polygons, thread_count);

		// PO层多边形切割AA层多边形
		robin_hood::unordered_map<int, std::vector<Edge>> po_cut_edges; // 各个po对应的切割边, 此时key使用的是po多边形数组下标, value为切割边列表, 切割边节点使用的是aa多边形数组下标
		po_cut_edges.reserve(po_polygons.size());
		Cut_AA_Parallel(po_polygons, aa_polygons, po_cut_edges, thread_count);

		// 由于PO层和AA层数量变化和id被修改，故按序重建他们的id，注意我们输入时保证了PO和AA分别是倒二和倒一层，他们的id也是排在末尾的，所以不会影响其他层
		int po_start_id = input.polygon_id_range_in_layer[po_layer_id].first; // po起始id
		int new_total_polygon_size = po_start_id + po_polygons.size() + aa_polygons.size();
		input.total_polygon = new_total_polygon_size;
		input.polygons.resize(new_total_polygon_size);

		// 插入合并后的PO层多边形,并更新信息
		for (int i = 0; i < po_polygons.size(); i++) {
			input.polygons[po_start_id + i] = std::move(po_polygons[i]);
			input.polygons[po_start_id + i].id = po_start_id + i;
		}
		input.polygon_id_range_in_layer[po_layer_id] = Range(po_start_id, po_start_id + po_polygons.size() - 1);

		int aa_start_id = input.polygon_id_range_in_layer[po_layer_id].second + 1; // aa起始id
		// 插入合并后的AA层多边形,并更新信息
		for (int i = 0; i < aa_polygons.size(); i++) {
			input.polygons[aa_start_id + i] = std::move(aa_polygons[i]);
			input.polygons[aa_start_id + i].id = aa_start_id + i;
		}
		input.polygon_id_range_in_layer[aa_layer_id] = Range(aa_start_id, aa_start_id + aa_polygons.size() - 1);

		INFO_MSG( "After Merge and Cutting Polygons" )
		INFO_INSTR(input.PrintLayoutInfo();)

		// 新的id已确定，可更新po_cut_edges为使用对应的多边形id表示
		robin_hood::unordered_map<int, std::vector<Edge>> updated_po_cut_edges;
		updated_po_cut_edges.reserve(po_cut_edges.size());  // 预分配

		for (auto& [po_index, edges] : po_cut_edges) {
			// 直接在新映射表中使用正确ID，避免中间容器
			auto& new_edges = updated_po_cut_edges[po_start_id + po_index];
			new_edges.reserve(edges.size());  // 预分配每条边的列表

			for (auto& edge : edges) {  // 使用新的多边形id
				new_edges.emplace_back(
					aa_start_id + edge.first,
					aa_start_id + edge.second
				);
			}
		}

		// 返回结果
		return updated_po_cut_edges;
	}
private:
	// 并行版本：合并PO层重叠多边形
	void Merge_PO_Parallel(std::vector<Polygon>& po_polygons, int thread_count) {
		std::vector<Polygon*> po_polygons_ptr;
		po_polygons_ptr.reserve(po_polygons.size());
		// 重排id, 与下标对应，并提取指针
		for (int i = 0; i < po_polygons.size(); i++) {
			 po_polygons[i].id = i;
			 po_polygons_ptr.push_back(&po_polygons[i]);
		}
		// 建立空间索引
		std::string name = input.layer_id_to_name[input.gate_rule.first];
		QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, "PO:" + name);
		quad_tree->CreatIndexParallel(std::move(po_polygons_ptr), thread_count);
		// 相交检测求边
		std::vector<Edge> edges = getEdgeofQuadTree_mergePO_Parallel(quad_tree, thread_count);
		// 并查集获取所有联通分量
		UnionFindSet unionfs(po_polygons.size());
		for (auto& e : edges) unionfs.join(e.first, e.second);
		std::vector<std::vector<int>> components = unionfs.getComponents();

		// 合并每个联通分量的多边形
		std::vector<Polygon> po_merged_polygons(components.size());
    	#pragma omp parallel num_threads(thread_count) if(components.size() > 10000)
    	{
    	    // 每个线程处理自己的组件子集
    	    #pragma omp for schedule(dynamic, 50) nowait
    	    for (int i = 0; i < components.size(); i++) {
    	        auto& comp = components[i];
    	        if (comp.size() == 1) {
    	            // 直接赋值，无需临界区
    	            po_merged_polygons[i] = std::move(po_polygons[comp[0]]);
    	        }
				else {
    	            Vertexs& po_vertex1 = po_polygons[comp[0]].vertex;
    	            mbsoCore.setResultMPS(po_vertex1);
					// 序列式求并集
    	            for (int j = 1; j < comp.size(); ++j) {
    	                int idx = comp[j];
    	                Vertexs& po_vertex2 = po_polygons[idx].vertex;
    	                mbsoCore.join(po_vertex2);
    	            }

    	            std::vector<std::vector<MPoint_2>> merged_poly_set = mbsoCore.getResult();
    	            assert(merged_poly_set.size() == 1 && "合并后多边形应为单一多边形");

    	            // 直接创建多边形并赋值
    	            Polygon& new_poly = po_merged_polygons[i];
    	            new_poly.layer_id = po_polygons[comp[0]].layer_id;
    	            new_poly.vertex = std::move(merged_poly_set[0]);
    	            new_poly.rect = input.GetRectofPolygon(new_poly.vertex);
    	        }
    	    }
    	}
		// 更新po_polygons
		po_polygons = std::move(po_merged_polygons);
		// 释放四叉树内存
		delete quad_tree;
	}

	// 并行版本：PO层多边形切割AA层多边形
	void Cut_AA_Parallel(std::vector<Polygon>& po_polygons, std::vector<Polygon>& aa_polygons,
		robin_hood::unordered_map<int, std::vector<Edge>>& po_cut_edges, int thread_count)
	{
		// PO和AA合并重排id, 与下标对应
		std::vector<Polygon*> poly_ptr;
		poly_ptr.reserve(po_polygons.size() + aa_polygons.size());
		for (int i = 0; i < po_polygons.size(); i++) {
			po_polygons[i].id = i;
			poly_ptr.push_back(&po_polygons[i]);
		}
		for (int i = 0; i < aa_polygons.size(); i++) {
			aa_polygons[i].id = po_polygons.size() + i;
			poly_ptr.push_back(&aa_polygons[i]);
		}
		// 两层合并建立空间索引
		std::string name = input.layer_id_to_name[input.gate_rule.first] +"-"+ input.layer_id_to_name[input.gate_rule.second];
		QuadTree* quad_tree = new QuadTree(input.layout, MAX_DEPTH, MAX_DATA_NUM, "PO-AA:" + name);
		quad_tree->CreatIndexParallel(poly_ptr, thread_count);
		// 相交检测求边，要求边的两点属于不同层
		std::vector<Edge> edges = getEdgeofQuadTree_cutAA_Parallel(quad_tree, thread_count);

		// 对每个AA多边形，记录其被哪些PO多边形切割
		robin_hood::unordered_map<int, std::vector<int>> aa_cut_by_po; // key为AA多边形此时id, value为切割它的PO多边形此时id
		aa_cut_by_po.reserve(aa_polygons.size());
		for (auto& e : edges) {
			// 已保证边顺序e.first < e.second , 所以e.first为PO多边形, e.second为AA多边形
			int po_id = e.first;
			int aa_id = e.second;
			aa_cut_by_po[aa_id].push_back(po_id);
		}

		// 对每个AA多边形，执行切割
		// 预计算总空间需求
    	size_t total_estimated_polys = aa_polygons.size() * 7; // 预估平均一个被切成七个
    	std::vector<Polygon> aa_cut_polygons;
    	aa_cut_polygons.reserve(total_estimated_polys);

    	// 使用线程局部存储，避免临界区竞争
    	#pragma omp parallel num_threads(thread_count)
    	{
    	    // 每个线程有自己的结果容器
    	    std::vector<Polygon> local_polys;
    	    robin_hood::unordered_map<int, std::vector<Edge>> local_edges;

    	    // 预分配空间，避免动态扩容
    	    local_polys.reserve(total_estimated_polys / thread_count + 1000);
    	    local_edges.reserve(po_polygons.size() / thread_count + 100);

    	    #pragma omp for schedule(dynamic, 50) nowait
    	    for (int i = 0; i < aa_polygons.size(); i++) {
    	        Polygon& aa_poly = aa_polygons[i];
    	        int aa_id = aa_poly.id;
    	        auto it = aa_cut_by_po.find(aa_id);
    	        // 未被任何PO切割
    	        if (it == aa_cut_by_po.end()) {
    	            local_polys.emplace_back(std::move(*poly_ptr[aa_id]));
    	            continue;
    	        }
    	        // 若被PO切割
    	        std::vector<int>& cutting_po = it->second;
    	        size_t start_index = local_polys.size(); // 记录切割前的位置

    	        // 被一个PO切割
    	        if (cutting_po.size() == 1) {
    	            // 设置 AA 多边形
    	            Vertexs& aa_vertex = poly_ptr[aa_id]->vertex;
    	            mbsoCore.setResultMPS(aa_vertex);
    	            // 求差集切割
    	            Vertexs& po_vertex = poly_ptr[cutting_po[0]]->vertex;
    	            mbsoCore.difference(po_vertex);
    	            // 获取结果
    	            std::vector<std::vector<MBSO::MPoint_2>> cut_poly_set = mbsoCore.getResult();
    	            assert(cut_poly_set.size() > 1 && "切割后多边形应不止一个");

    	            // 切割后多边形转回自定义Polygon类
    	            for (auto& poly : cut_poly_set) {
    	                // reverse(poly.begin(), poly.end());
    	                Polygon new_poly;
    	                new_poly.layer_id = poly_ptr[aa_id]->layer_id;
    	                new_poly.vertex = std::move(poly);
    	                new_poly.rect = input.GetRectofPolygon(new_poly.vertex);
    	                local_polys.emplace_back(std::move(new_poly));
    	            }

    	            // 链式记录该PO的切割边
    	            for (size_t j = start_index + 1; j < local_polys.size(); j++) {
    	                local_edges[cutting_po[0]].emplace_back(j - 1, j);
    	            }
    	        }
    	        // 被多个PO切割
    	        else {
    	            // 设置 AA 多边形
    	            Vertexs& aa_vertex = poly_ptr[aa_id]->vertex;
    	            mbsoCore.setResultMPS(aa_vertex);
    	            // 构造多个po的多边形集合
    	            std::vector<std::vector<MPoint_2>> mpolys_2;
    	            mpolys_2.reserve(cutting_po.size());
    	            for (auto& po_id : cutting_po) {
    	                Vertexs& po_vertex = poly_ptr[po_id]->vertex;
    	                mpolys_2.push_back(po_vertex);
    	            }

    	            robin_hood::unordered_map<int, std::vector<int>> po_cut_nodes;
    	            mbsoCore.difference(mpolys_2, po_cut_nodes);

    	            // 获取结果
    	            std::vector<std::vector<MBSO::MPoint_2>> cut_poly_set = mbsoCore.getResult();
    	            assert(cut_poly_set.size() > 1 && "切割后多边形应不止一个");

    	            // 切割后多边形转回自定义Polygon类
    	            for (auto& poly : cut_poly_set) {
    	                // reverse(poly.begin(), poly.end());
    	                Polygon new_poly;
    	                new_poly.layer_id = poly_ptr[aa_id]->layer_id;
    	                new_poly.vertex = std::move(poly);
    	                new_poly.rect = input.GetRectofPolygon(new_poly.vertex);
    	                local_polys.emplace_back(std::move(new_poly));
    	            }

    	            // 记录切割边
    	            for (auto& item : po_cut_nodes) {
    	                int po_idx = item.first;
    	                int po_id = cutting_po[po_idx];
    	                std::vector<int>& nodes = item.second;
						// 链式记录该PO的切割边
    	                for (size_t j = 1; j < nodes.size(); j++) {
    	                    size_t idx1 = start_index + nodes[j-1];
    	                    size_t idx2 = start_index + nodes[j];
    	                    local_edges[po_id].emplace_back(idx1, idx2);
    	                }
    	            }
    	        }
    	    }

    	    // 将本地结果合并到全局（需要临界区保护）
    	    #pragma omp critical
    	    {
    	        // 计算当前线程结果在全局中的偏移量
    	        size_t global_offset = aa_cut_polygons.size();
    	        // 移动语义批量插入多边形
            	aa_cut_polygons.insert(aa_cut_polygons.end(),
                                  	std::make_move_iterator(local_polys.begin()),
                                  	std::make_move_iterator(local_polys.end()));

    	        // 调整边索引并合并
            	for (auto& pair : local_edges) {
            	    int po_id = pair.first;
            	    auto& edge_vec = po_cut_edges[po_id];

            	    // 预分配空间
            	    if (edge_vec.capacity() - edge_vec.size() < pair.second.size()) {
            	        edge_vec.reserve(edge_vec.size() + pair.second.size());
            	    }

            	    for (Edge& edge : pair.second) {
            	        edge.first += global_offset;
            	        edge.second += global_offset;
            	        edge_vec.push_back(edge);
            	    }
            	}
    	    }
    	}

    	// 更新结果
    	aa_polygons = std::move(aa_cut_polygons);

    	// 释放四叉树内存
    	delete quad_tree;
	}

	// 并行版本：执行给定索引的多边形相交检测，获取其边的集合
	std::vector<std::pair<int, int>> getEdgeofQuadTree_mergePO_Parallel(QuadTree* qtree, int thread_count) {
		std::vector<std::pair<int, int>> edges;
		edges.reserve(500000);
		// 收集空间索引
		std::vector<QuadTreeNode*> leafNode;
		INFO_MSG( "Handling QuadTree : " << qtree->_name )
		qtree->GetAllLeafNode(leafNode);

		// 并行处理每个叶子节点
    	#pragma omp parallel num_threads(thread_count)
    	{
    	    // 每个线程有自己的局部边集合
    	    std::vector<std::pair<int, int>> local_edges;
    	    local_edges.reserve(1000); // 预分配局部内存

    	    // 并行处理每个叶子节点
    	    #pragma omp for schedule(dynamic)
    	    for (int idx = 0; idx < leafNode.size(); idx++) {
				auto& lfd = leafNode[idx]->_datas;
    	        if (lfd.size() < 2) continue;

    	        // 处理当前叶子节点内的多边形对
    	        for (int i = 0; i < (int)lfd.size(); i++) {
    	            Polygon* a = lfd[i];
    	            for (int j = i + 1; j < (int)lfd.size(); j++) {
    	                Polygon* b = lfd[j];
    	                // 使用曼哈顿多边形相交检测
    	                if (ManhattanIntersectDetector::manhattanPolygonsIntersect(a, b)) {
    	                    // 规范边（确保 first < second）
    	                    int min_id = std::min(a->id, b->id);
    	                    int max_id = std::max(a->id, b->id);
    	                    local_edges.emplace_back(min_id, max_id);
    	                }
    	            }
    	        }
    	    }

    	    // 合并局部结果到全局（使用临界区）
    	    #pragma omp critical
    	    {
    	        edges.insert(edges.end(), local_edges.begin(), local_edges.end());
    	    }
    	}

    	// 全局排序并去重
    	std::sort(edges.begin(), edges.end());
    	auto last = std::unique(edges.begin(), edges.end());
    	edges.erase(last, edges.end());

    	return edges;
	}

	// 并行版本：执行给定索引的多边形相交检测，获取其边的集合
	std::vector<std::pair<int, int>> getEdgeofQuadTree_cutAA_Parallel(QuadTree* qtree, int thread_count) {
		std::vector<std::pair<int, int>> edges;
		edges.reserve(500000);
		// 收集空间索引
		std::vector<QuadTreeNode*> leafNode;
		INFO_MSG( "Handling QuadTree : " << qtree->_name )
		qtree->GetAllLeafNode(leafNode);

		// 并行处理每个叶子节点
    	#pragma omp parallel num_threads(thread_count)
    	{
    	    // 每个线程有自己的局部边集合
    	    std::vector<std::pair<int, int>> local_edges;
    	    local_edges.reserve(1000); // 预分配局部内存

    	    // 并行处理每个叶子节点
    	    #pragma omp for schedule(dynamic)
    	    for (int idx = 0; idx < leafNode.size(); idx++) {
				auto& lfd = leafNode[idx]->_datas;
    	        if (lfd.size() < 2) continue;

    	        // 处理当前叶子节点内的多边形对
    	        for (int i = 0; i < (int)lfd.size(); i++) {
    	            Polygon* a = lfd[i];
    	            for (int j = i + 1; j < (int)lfd.size(); j++) {
    	                Polygon* b = lfd[j];
						if (a->layer_id == b->layer_id) continue; // 同层不检测
    	                // 使用曼哈顿多边形相交检测
    	                if (ManhattanIntersectDetector::manhattanPolygonsIntersect(a, b)) {
    	                    // 规范边（确保 first < second）
    	                    int min_id = std::min(a->id, b->id);
    	                    int max_id = std::max(a->id, b->id);
    	                    local_edges.emplace_back(min_id, max_id);
    	                }
    	            }
    	        }
    	    }

    	    // 合并局部结果到全局（使用临界区）
    	    #pragma omp critical
    	    {
    	        edges.insert(edges.end(), local_edges.begin(), local_edges.end());
    	    }
    	}

    	// 全局排序并去重
    	std::sort(edges.begin(), edges.end());
    	auto last = std::unique(edges.begin(), edges.end());
    	edges.erase(last, edges.end());

		return edges;
	}

#pragma endregion
};