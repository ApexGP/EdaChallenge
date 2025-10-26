#pragma once

#include "public.h"
#include "Input.hpp"
#include "SpaceIndex.hpp"
#include "ManhattanIntersectDetector.hpp"
#include <vector>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <unordered_set>
#include <numeric>

/**
 * @brief Intersection detection method enumeration
 */
enum class IntersectionMethod {
    MANHATTAN_COMPLETE,         // Complete Manhattan edge intersection detection
    BATCH_GRID,                 // Batch grid processing
};

/**
 * @brief Intersection detection statistics
 */
struct IntersectionStats {
    size_t total_leaf_nodes = 0;
    size_t total_polygon_pairs = 0;
    size_t manhattan_complete_used = 0;
    size_t batch_grid_used = 0;
    size_t prefilter_used = 0;
    size_t intersections_found = 0;
    double total_time_ms = 0.0;

    void reset() {
        total_leaf_nodes = 0;
        total_polygon_pairs = 0;
        manhattan_complete_used = 0;
        batch_grid_used = 0;
        prefilter_used = 0;
        intersections_found = 0;
        total_time_ms = 0.0;
    }

    void print() const {
        std::cout << "\n=== Intersection Detection Statistics ===" << std::endl;
        std::cout << "Total leaf nodes: " << total_leaf_nodes << std::endl;
        std::cout << "Total polygon pairs: " << total_polygon_pairs << std::endl;
        std::cout << "Intersections found: " << intersections_found << std::endl;
        std::cout << "Total time: " << total_time_ms << " ms" << std::endl;

        if (total_leaf_nodes > 0) {
            std::cout << "Algorithm usage:" << std::endl;
            std::cout << "  Manhattan Complete: " << manhattan_complete_used << " ("
                << (double)manhattan_complete_used / total_leaf_nodes * 100 << "%)" << std::endl;
            std::cout << "  Batch Grid: " << batch_grid_used << " ("
                << (double)batch_grid_used / total_leaf_nodes * 100 << "%)" << std::endl;
            std::cout << "  Prefilter: " << prefilter_used << " ("
                << (double)prefilter_used / total_leaf_nodes * 100 << "%)" << std::endl;
        }

        if (total_polygon_pairs > 0 && intersections_found > 0) {
            std::cout << "Intersection rate: "
                << (double)intersections_found / total_polygon_pairs * 100 << "%" << std::endl;
        }
        std::cout << "=========================================" << std::endl;
    }
};

/**
 * @brief Main class for intersection detection
 */
class Intersect {
private:
    Input& input;
    SpaceIndex& spaceIndex;

    /* For 完全建图 */
    mutable IntersectionStats stats;
    // Current intersection detection method
    IntersectionMethod method = IntersectionMethod::MANHATTAN_COMPLETE;

    /* For 延迟建图 */
    // Buffer for collecting leaf nodes
    std::vector<QuadTreeNode*> leaf_buffer;
    // Cache for neighbor candidate detection
    robin_hood::unordered_set<int> neighbor_candidate_cache;
    // Statistics for lazy neighbor detection
    size_t lazy_neighbor_calls = 0;
    size_t lazy_neighbor_candidates = 0;
    size_t lazy_neighbor_enqueues = 0;
    size_t lazy_neighbor_cache_hits = 0;
    size_t lazy_neighbor_duplicates = 0;

public:
    /**
     * @brief Constructor
     * @param _input Reference to input data
     * @param _spaceIndex Reference to spatial index
     */
    Intersect(Input& _input, SpaceIndex& _spaceIndex) : input(_input), spaceIndex(_spaceIndex) {}

    /**
     * @brief Destructor - flushes statistics
     */
    ~Intersect() {}

    /**
     * @brief Gets all intersecting edges between polygons
     * @return Vector of polygon ID pairs that intersect
     */
    std::vector<std::pair<int, int>> getAllEdge() {
        Timer total_timer;
        stats.reset();

        std::vector<std::pair<int, int>> edges;
        edges.reserve(500000);
        const std::vector<QuadTree*>& quad_trees = spaceIndex.GetSpaceIndex();

        std::cout << "Using intersection method: " << getMethodName() << std::endl;

        // Process based on selected method
        switch (method) {
        case IntersectionMethod::MANHATTAN_COMPLETE:
            processWithManhattanComplete(quad_trees, edges);
            break;
        //case IntersectionMethod::BATCH_GRID:
        //    processWithBatchGrid(quad_trees, edges);
        //    break;
        }

        // Remove duplicate edges
        removeDuplicateEdges(edges);

        // Print statistics
        stats.total_time_ms = total_timer.ElapsedMs();
        stats.print();

        return edges;
    }

    // 并行版本：获取所有相交边
    std::vector<std::pair<int, int>> getAllEdgeParallel(int thread_count) {
        Timer total_timer;
        stats.reset();

        std::vector<std::pair<int, int>> edges;
        edges.reserve(500000);
        const std::vector<QuadTree*>& quad_trees = spaceIndex.GetSpaceIndex();

        std::cout << "Using intersection method: " << getMethodName() << std::endl;

        // Process based on selected method
        switch (method) {
        case IntersectionMethod::MANHATTAN_COMPLETE:
            processWithManhattanCompleteParallel(quad_trees, edges, thread_count);
            break;
        //case IntersectionMethod::BATCH_GRID:
        //    processWithBatchGrid(quad_trees, edges);
        //    break;
        }

        // Remove duplicate edges
        removeDuplicateEdges(edges);

        // Print statistics
        stats.total_time_ms = total_timer.ElapsedMs();
        stats.print();

        return edges;
    }

    /**
     * @brief Gets the name of the current method
     * @return Method name string
     */
    std::string getMethodName() const {
        switch (method) {
        case IntersectionMethod::MANHATTAN_COMPLETE: return "Manhattan Complete Detection";
        case IntersectionMethod::BATCH_GRID: return "Batch Grid";
        default: return "Unknown";
        }
    }

    /**
     * @brief Gets intersection statistics
     * @return Reference to statistics object
     */
    const IntersectionStats& getStats() const { return stats; }

#pragma region for_lazy_bfs
    /* ===================== For 延迟建图 begin =====================*/
    /**
    * @brief Gets neighboring polygons that intersect with the given polygon using lazy evaluation
    * @param polygon_id ID of the source polygon
    * @param bfs_visted BFS Visit tracking array
    * @param neighbors Output vector for neighbor IDs
    */
    void GetNeighborsLazy(int polygon_id, const std::vector<bool>& bfs_visted, std::vector<int>& neighbors) {
        neighbors.clear();

        Polygon* source = input.polygons[polygon_id];
        neighbor_candidate_cache.clear();
        ++lazy_neighbor_calls;

        // 获取与该多边形所在层有关联的所有四叉树
        const auto& quad_trees = spaceIndex.GetQuadTreesForLayer(source->layer_id);
        for (auto* qtree : quad_trees) {
            if (!qtree) continue;

            // Collect leaves that intersect with the source polygon's bounding box
            leaf_buffer.clear();
            qtree->CollectIntersectLeaves(source->rect, leaf_buffer);
            // 遍历每个叶节点
            for (auto* leaf : leaf_buffer) {
                // Direct checking for light leaves
                for (auto* candidate : leaf->_datas) {
                    ++lazy_neighbor_candidates;
                    const int candidate_id = candidate->id;

                    if (candidate_id == polygon_id) continue; // 是自身
                    if (bfs_visted[candidate_id]) continue;   // bfs已访问过

                    //if (!neighbor_candidate_cache.insert(candidate_id).second) { // 重复邻居 ps:不去重更快
                    //    ++lazy_neighbor_duplicates;
                    //    continue;
                    //}

                    // Detailed Manhattan intersection check
                    if (ManhattanIntersectDetector::manhattanPolygonsIntersect(source, candidate)) {
                        neighbors.push_back(candidate_id);
                        ++lazy_neighbor_enqueues;
                    }
                }
            }
        }
    }

    /**
     * @brief Flushes and prints statistics
     */
    void FlushStats() const {
        if (lazy_neighbor_calls > 0) {
            std::cout << "[LazyBFS] neighbor stats - calls=" << lazy_neighbor_calls
                << " candidates=" << lazy_neighbor_candidates
                << " enqueued=" << lazy_neighbor_enqueues
                << " cache_hits=" << lazy_neighbor_cache_hits
                << " duplicates=" << lazy_neighbor_duplicates << std::endl;
        }
    }

    /* ===================== For 延迟建图 end =====================*/
#pragma endregion

private:
    // ========== Intersection Detection Method Implementations ==========

    /**
     * @brief Process using complete Manhattan edge intersection detection
     * @param quad_trees Vector of quad trees to process
     * @param edges Output vector for intersecting edges
     */
    void processWithManhattanComplete(const std::vector<QuadTree*>& quad_trees, std::vector<std::pair<int, int>>& edges) {
        UnionFindSet ufs(100);
        std::vector<std::vector<Polygon*>> leafData;
        leafData.reserve(1000000);

        for (auto& qtree : quad_trees) {
            leafData.clear();
            std::cout << "Handling QuadTree : " << qtree->_name << std::endl;

            // 处理延迟索引划分
            if (qtree->_root->_divided == false && qtree->_root->_datas.size() == 0) {
                spaceIndex.CreatQuadTreeIndex(qtree);
            }

            qtree->GetAllLeafData(leafData);

            for (const auto& lfd : leafData) {
                const size_t local_size = lfd.size();
                if (local_size < 2) {
                    continue;
                }

                stats.total_leaf_nodes++;
                stats.total_polygon_pairs += (local_size * (local_size - 1)) / 2;
                stats.manhattan_complete_used++;

                // 建立小型并查集
				if (lfd.size() > ufs.getSize())
					ufs = UnionFindSet(lfd.size() * 2);
				else
					ufs.init();

				// n方遍历 内部的多边形对
				for (int i = 0; i < (int)lfd.size(); i++) {
					Polygon* a = lfd[i];
					for (int j = i + 1; j < (int)lfd.size(); j++) {
						Polygon* b = lfd[j];
						if (ufs.find(i) == ufs.find(j)) continue; // 已经连通的跳过
						// 使用曼哈顿多边形相交检测
						if (ManhattanIntersectDetector::manhattanPolygonsIntersect(a, b)) {
							edges.emplace_back(a->id, b->id);
							ufs.join(i, j);
							stats.intersections_found++;
						}
					}
				}
            }
            // 处理完该四叉树，后续不再使用，释放其内存
            qtree->clear();
        }
    }

    // 并行版本openMP：曼哈顿相交检测建边
    void processWithManhattanCompleteParallel(const std::vector<QuadTree*>& quad_trees, std::vector<std::pair<int, int>>& edges, int thread_count) {
        // 设置最大线程数
        omp_set_num_threads(thread_count);
        
        // 用于合并边的互斥锁
        std::mutex edges_mutex;
        
        // 并行处理每个四叉树
        #pragma omp parallel for schedule(dynamic)
        for (int idx = 0; idx < quad_trees.size(); ++idx) {
            QuadTree* qtree = quad_trees[idx];
            UnionFindSet ufs(100);  // 每个线程有自己的并查集
            std::vector<std::vector<Polygon*>> leafData;
            leafData.reserve(1000000);

            // 处理延迟索引划分
            if (qtree->_root->_divided == false && qtree->_root->_datas.size() == 0) {
                spaceIndex.CreatQuadTreeIndex(qtree);
            }

            qtree->GetAllLeafData(leafData);

            // 线程本地存储边缘结果
            std::vector<std::pair<int, int>> local_edges;

            for (const auto& lfd : leafData) {
                const size_t local_size = lfd.size();
                if (local_size < 2) {
                    continue;
                }

                // 调整并查集大小
                if (lfd.size() > ufs.getSize()) {
                    ufs = UnionFindSet(lfd.size() * 2);
                } else {
                    ufs.init();
                }

                // 检查多边形相交
                for (int i = 0; i < (int)lfd.size(); i++) {
                    Polygon* a = lfd[i];
                    for (int j = i + 1; j < (int)lfd.size(); j++) {
                        Polygon* b = lfd[j];
                        if (ufs.find(i) == ufs.find(j)) continue;
                        if (ManhattanIntersectDetector::manhattanPolygonsIntersect(a, b)) {
                            local_edges.emplace_back(a->id, b->id);
                            ufs.join(i, j);
                        }
                    }
                }
            }

            // 安全合并边结果
            {
                std::lock_guard<std::mutex> lock(edges_mutex);
                edges.insert(edges.end(), local_edges.begin(), local_edges.end());
            }

            // 释放当前四叉树内存
            qtree->clear();
        }
    }

    // 并行版本-线程池：曼哈顿相交检测建边
    // void processWithManhattanCompleteParallel(const std::vector<QuadTree*>& quad_trees, std::vector<std::pair<int, int>>& edges, int thread_count) {
    //     // 创建线程池
    //     NaiveThreadPool pool(thread_count);
    //     // 用于合并边的互斥锁
    //     std::mutex edges_mutex;
        
    //     for (int idx = 0; idx < quad_trees.size(); ++idx) {
    //         QuadTree* qtree = quad_trees[idx];
    //         pool.enqueue([this, qtree, &edges, &edges_mutex] {

    //             UnionFindSet ufs(100);  // 每个线程有自己的并查集
    //             std::vector<std::vector<Polygon*>> leafData;
    //             leafData.reserve(1000000);

    //             // 处理延迟索引划分
    //             if (qtree->_root->_divided == false && qtree->_root->_datas.size() == 0) {
    //                 spaceIndex.CreatQuadTreeIndex(qtree);
    //             }

    //             qtree->GetAllLeafData(leafData);

    //             // 线程本地存储边缘结果
    //             std::vector<std::pair<int, int>> local_edges;

    //             for (const auto& lfd : leafData) {
    //             const size_t local_size = lfd.size();
    //             if (local_size < 2) {
    //                 continue;
    //             }

    //             // 调整并查集大小
    //             if (lfd.size() > ufs.getSize()) {
    //                 ufs = UnionFindSet(lfd.size() * 2);
    //             } else {
    //                 ufs.init();
    //             }

    //             // 检查多边形相交
    //             for (int i = 0; i < (int)lfd.size(); i++) {
    //                 Polygon* a = lfd[i];
    //                 for (int j = i + 1; j < (int)lfd.size(); j++) {
    //                     Polygon* b = lfd[j];
    //                     if (ufs.find(i) == ufs.find(j)) continue;
    //                     if (ManhattanIntersectDetector::manhattanPolygonsIntersect(a, b)) {
    //                         local_edges.emplace_back(a->id, b->id);
    //                         ufs.join(i, j);
    //                     }
    //                 }
    //             }
    //         }

    //             // 安全合并边结果
    //             {
    //                 std::lock_guard<std::mutex> lock(edges_mutex);
    //                 edges.insert(edges.end(), local_edges.begin(), local_edges.end());
    //             }

    //             // 释放当前四叉树内存
    //             qtree->clear();
    //         });
    //     }
    // }

    /**
     * @brief Process using batch grid method
     * @param quad_trees Vector of quad trees to process
     * @param edges Output vector for intersecting edges
     */
    void processWithBatchGrid(const std::vector<QuadTree*>& quad_trees, std::vector<std::pair<int, int>>& edges) {
        BatchGridIntersectDetector bgid;
        std::vector<std::vector<Polygon*>> leafData;
        std::vector<Rect> leafRect;
        leafData.reserve(1000000);
        leafRect.reserve(1000000);

        for (auto& qtree : quad_trees) {
            // Collect spatial data
            leafData.clear();
            leafRect.clear();
            std::cout << "Handling QuadTree : " << qtree->_name << std::endl;

            // 处理延迟索引划分
            if (qtree->_root->_divided == false && qtree->_root->_datas.size() == 0) {
                spaceIndex.CreatQuadTreeIndex(qtree);
            }

            qtree->GetAllLeafData(leafData, leafRect);

            // Process each leaf based on spatial bounds
            for (size_t i = 0; i < leafData.size(); ++i) {
                const auto& lfd = leafData[i];
                const auto& rect = leafRect[i];
                if (lfd.size() < 2) continue;

                stats.total_leaf_nodes++;
                stats.total_polygon_pairs += (lfd.size() * (lfd.size() - 1)) / 2;
                stats.batch_grid_used++;

                // Use batch grid detector
                size_t edges_nums_before = edges.size();
                bgid.batchManhattanPolygonsIntersect(rect, lfd, edges);
                stats.intersections_found += (edges.size() - edges_nums_before);
            }
            // 处理完该四叉树，后续不再使用，释放其内存
            qtree->clear();
        }
    }

    /**
     * @brief Removes duplicate edges from the result
     * @param edges Vector of edges to deduplicate
     */
    void removeDuplicateEdges(std::vector<std::pair<int, int>>& edges) {
        // Normalize edges (ensure first < second)
        for (auto& edge : edges) {
            if (edge.first > edge.second) {
                std::swap(edge.first, edge.second);
            }
        }

        // Sort and remove duplicates
        std::sort(edges.begin(), edges.end());
        edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    }
};