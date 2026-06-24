#pragma once

#include "public.h"
#include "Input.hpp"
#include "SpaceIndex.hpp"
#include "ManhattanIntersectDetector.hpp"
#include <vector>
#include <cstdint>
#include <algorithm>

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
    INFO_INSTR(mutable IntersectionStats stats;)
    // Current intersection detection method
    IntersectionMethod method = IntersectionMethod::MANHATTAN_COMPLETE;

    /* For 延迟建图 */
    // Cache for neighbor candidate detection
    robin_hood::unordered_set<int> neighbor_candidate_cache;
    // Statistics for lazy neighbor detection
    INFO_INSTR(size_t lazy_neighbor_calls = 0;)
    INFO_INSTR(size_t lazy_neighbor_candidates = 0;)
    INFO_INSTR(size_t lazy_neighbor_enqueues = 0;)
    INFO_INSTR(size_t lazy_neighbor_cache_hits = 0;)
    INFO_INSTR(size_t lazy_neighbor_duplicates = 0;)

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
        INFO_INSTR(Timer total_timer;)
        INFO_INSTR(stats.reset();)

        std::vector<std::pair<int, int>> edges;
        edges.reserve(500000);
        const std::vector<QuadTree*>& quad_trees = spaceIndex.GetSpaceIndex();

        INFO_MSG( "Using intersection method: " << getMethodName() )

        // Process based on selected method
        switch (method) {
        case IntersectionMethod::MANHATTAN_COMPLETE:
            processWithManhattanComplete(quad_trees, edges);
            break;
        //case IntersectionMethod::BATCH_GRID:
        //    processWithBatchGrid(quad_trees, edges);
        //    break;
        default:
            break;
        }

        // Remove duplicate edges
        removeDuplicateEdges(edges);

        // Print statistics
        INFO_INSTR(stats.total_time_ms = total_timer.ElapsedMs();)
        INFO_INSTR(stats.print();)

        return edges;
    }

    // 并行版本：获取所有相交边
    std::vector<std::pair<int, int>> getAllEdgeParallel(int thread_count) {
        INFO_INSTR(Timer total_timer;)
        INFO_INSTR(stats.reset();)

        std::vector<std::pair<int, int>> edges;
        edges.reserve(500000);
        const std::vector<QuadTree*>& quad_trees = spaceIndex.GetSpaceIndex();

        INFO_MSG( "Using intersection method: " << getMethodName() )

        // Process based on selected method
        switch (method) {
        case IntersectionMethod::MANHATTAN_COMPLETE:
            processWithManhattanCompleteParallel(quad_trees, edges, thread_count);
            break;
        //case IntersectionMethod::BATCH_GRID:
        //    processWithBatchGrid(quad_trees, edges);
        //    break;
        default:
            break;
        }

        // Remove duplicate edges
        removeDuplicateEdges(edges);

        // Print statistics
        INFO_INSTR(stats.total_time_ms = total_timer.ElapsedMs();)
        INFO_INSTR(stats.print();)

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
    INFO_INSTR(const IntersectionStats& getStats() const { return stats; })

#pragma region for_lazy_bfs
    /* ===================== For 延迟建图 begin =====================*/
    /**
    * @brief Gets neighboring polygons that intersect with the given polygon using lazy evaluation
    * @param polygon_id ID of the source polygon
    * @param bfs_visted BFS Visit tracking array
    * @param neighbors Output vector for neighbor IDs
    */
    void GetNeighborsLazy(int polygon_id, const std::vector<uint8_t>& bfs_visted, std::vector<int>& neighbors) {
        neighbors.clear();

        Polygon* source = &input.polygons[polygon_id];
        neighbor_candidate_cache.clear();
        INFO_INSTR(++lazy_neighbor_calls;)

        // 获取该多边形所在层关联的四叉树
        const auto& quad_trees = spaceIndex.GetQuadTreesForLayer(source->layer_id);
        for (auto* qtree : quad_trees) {
            if (!qtree) continue;

            // 直接递归收集叶节点并处理，避免中间缓冲区
            CollectAndProcessLeaves(qtree->_root, source, bfs_visted, neighbors);
        }
    }

    // 并行版本，注意线程安全
    void GetNeighborsLazyParallel(int polygon_id, const std::vector<uint8_t>& bfs_visted, std::vector<int>& neighbors) {
        neighbors.clear();

        Polygon* source = &input.polygons[polygon_id];

        // 获取该多边形所在层关联的四叉树
        const auto& quad_trees = spaceIndex.GetQuadTreesForLayer(source->layer_id);
        thread_local robin_hood::unordered_set<int> candidate_cache_local;
        candidate_cache_local.clear();
        for (auto* qtree : quad_trees) {
            if (!qtree) continue;

            // 直接递归收集叶节点并处理，避免中间缓冲区
            CollectAndProcessLeavesParallel(qtree->_root, source, bfs_visted, neighbors, candidate_cache_local);
        }
    }

    /**
     * @brief Flushes and prints statistics
     */
    void FlushStats() const {
        INFO_INSTR(
            if (lazy_neighbor_calls > 0) {
            INFO_MSG( "[LazyBFS] neighbor stats - calls=" << lazy_neighbor_calls
                << " candidates=" << lazy_neighbor_candidates
                << " enqueued=" << lazy_neighbor_enqueues
                << " cache_hits=" << lazy_neighbor_cache_hits
                << " duplicates=" << lazy_neighbor_duplicates )
            }
        )
    }

    /* ===================== For 延迟建图 end =====================*/
#pragma endregion

private:
    // 直接递归遍历四叉树收集与source相交的叶节点，并在候选多边形上执行相交检测
    void CollectAndProcessLeaves(QuadTreeNode* node, Polygon* source,
                                  const std::vector<uint8_t>& bfs_visted,
                                  std::vector<int>& neighbors) {
        if (node->_divided) {
            const int xmid = node->_lt->_rect._xmax;
            const int ymid = node->_lt->_rect._ymin;
            const Rect& src_rect = source->rect;

            if (src_rect._xmin <= xmid) {
                if (src_rect._ymax >= ymid) CollectAndProcessLeaves(node->_lt, source, bfs_visted, neighbors);
                if (src_rect._ymin <= ymid) CollectAndProcessLeaves(node->_lb, source, bfs_visted, neighbors);
            }
            if (src_rect._xmax >= xmid) {
                if (src_rect._ymax >= ymid) CollectAndProcessLeaves(node->_rt, source, bfs_visted, neighbors);
                if (src_rect._ymin <= ymid) CollectAndProcessLeaves(node->_rb, source, bfs_visted, neighbors);
            }
        } else if (!node->_datas.empty()) {
            // 叶节点：直接处理候选多边形
            const int polygon_id = source->id;
            for (auto* candidate : node->_datas) {
                INFO_INSTR(++lazy_neighbor_candidates;)
                const int candidate_id = candidate->id;

                if (candidate_id == polygon_id) continue;
                if (bfs_visted[candidate_id]) continue;

                if (!neighbor_candidate_cache.insert(candidate_id).second) {
                    INFO_INSTR(++lazy_neighbor_duplicates;)
                    continue;
                }

                if (ManhattanIntersectDetector::manhattanPolygonsIntersect(source, candidate)) {
                    neighbors.push_back(candidate_id);
                    INFO_INSTR(++lazy_neighbor_enqueues;)
                }
            }
        }
    }

    // 并行安全版本：直接递归遍历四叉树并处理候选多边形（使用外部传入的去重缓存，原子访问visited）
    void CollectAndProcessLeavesParallel(QuadTreeNode* node, Polygon* source,
                                          const std::vector<uint8_t>& bfs_visted,
                                          std::vector<int>& neighbors,
                                          robin_hood::unordered_set<int>& candidate_cache) {
        if (node->_divided) {
            const int xmid = node->_lt->_rect._xmax;
            const int ymid = node->_lt->_rect._ymin;
            const Rect& src_rect = source->rect;

            if (src_rect._xmin <= xmid) {
                if (src_rect._ymax >= ymid) CollectAndProcessLeavesParallel(node->_lt, source, bfs_visted, neighbors, candidate_cache);
                if (src_rect._ymin <= ymid) CollectAndProcessLeavesParallel(node->_lb, source, bfs_visted, neighbors, candidate_cache);
            }
            if (src_rect._xmax >= xmid) {
                if (src_rect._ymax >= ymid) CollectAndProcessLeavesParallel(node->_rt, source, bfs_visted, neighbors, candidate_cache);
                if (src_rect._ymin <= ymid) CollectAndProcessLeavesParallel(node->_rb, source, bfs_visted, neighbors, candidate_cache);
            }
        } else if (!node->_datas.empty()) {
            const int polygon_id = source->id;
            for (auto* candidate : node->_datas) {
                const int candidate_id = candidate->id;

                if (candidate_id == polygon_id) continue;
                if (__atomic_load_n(&bfs_visted[candidate_id], __ATOMIC_RELAXED)) continue;
                if (!candidate_cache.insert(candidate_id).second) continue;

                if (ManhattanIntersectDetector::manhattanPolygonsIntersect(source, candidate)) {
                    neighbors.push_back(candidate_id);
                }
            }
        }
    }

    // ========== Intersection Detection Method Implementations ==========

    /**
     * @brief Process using complete Manhattan edge intersection detection
     * @param quad_trees Vector of quad trees to process
     * @param edges Output vector for intersecting edges
     */
    void processWithManhattanComplete(const std::vector<QuadTree*>& quad_trees, std::vector<std::pair<int, int>>& edges) {
        UnionFindSet ufs(100);
        std::vector<QuadTreeNode*> leafNode;
        leafNode.reserve(1000000);

        for (auto& qtree : quad_trees) {
            leafNode.clear();
            INFO_MSG( "Handling QuadTree : " << qtree->_name )

            qtree->GetAllLeafNode(leafNode);

            for (const auto& lfNode : leafNode) {
                const auto &lfd = lfNode->_datas;
                const size_t local_size = lfd.size();
                if (local_size < 2) {
                    continue;
                }

                INFO_INSTR(stats.total_leaf_nodes++;)
                INFO_INSTR(stats.total_polygon_pairs += (local_size * (local_size - 1)) / 2;)
                INFO_INSTR(stats.manhattan_complete_used++;)

                // 按需扩展并查集
				if (lfd.size() > static_cast<size_t>(ufs.getSize()))
					ufs = UnionFindSet(std::max(lfd.size() * 2, static_cast<size_t>(ufs.getSize() * 2)));
				else
					ufs.init();

				// n方遍历 内部的两两端点对
				for (int i = 0; i < (int)lfd.size(); i++) {
					Polygon* a = lfd[i];
					for (int j = i + 1; j < (int)lfd.size(); j++) {
						Polygon* b = lfd[j];
						if (ufs.find(i) == ufs.find(j)) continue; // 已经连通的跳过
						// 使用曼哈顿多边形相交检测
						if (ManhattanIntersectDetector::manhattanPolygonsIntersect(a, b)) {
							edges.emplace_back(a->id, b->id);
							ufs.join(i, j);
							INFO_INSTR(stats.intersections_found++;)
						}
					}
				}
            }
        }
    }

    // 并行版本openMP批量多边形相交检测建边
    void processWithManhattanCompleteParallel(const std::vector<QuadTree*>& quad_trees, std::vector<std::pair<int, int>>& edges, int thread_count) {
        std::mutex edges_mutex;

        // 预收集所有叶节点
        std::vector<QuadTreeNode*> all_leaf_nodes;
        for (int idx = 0; idx < quad_trees.size(); ++idx) {
            QuadTree* qtree = quad_trees[idx];
            std::vector<QuadTreeNode*> leaf_nodes;
            qtree->GetAllLeafNode(leaf_nodes);
            all_leaf_nodes.insert(all_leaf_nodes.end(), leaf_nodes.begin(), leaf_nodes.end());
        }

        // 批大小，可根据实际数据量调整
        const int batch_size = std::min(1000, std::max(10, (int)all_leaf_nodes.size() / (thread_count * 20)));

        #pragma omp parallel num_threads(thread_count)
        {
            UnionFindSet ufs(100);
            std::vector<std::pair<int, int>> thread_local_edges;
            thread_local_edges.reserve(50000);  // 预分配空间

            // 使用动态调度，每个线程处理一小批
            #pragma omp for schedule(dynamic, batch_size) nowait
            for (int leaf_idx = 0; leaf_idx < all_leaf_nodes.size(); ++leaf_idx) {
                QuadTreeNode* lfNode = all_leaf_nodes[leaf_idx];
                const auto &lfd = lfNode->_datas;
                const size_t local_size = lfd.size();

                if (local_size < 2) {
                    continue;
                }

                // 动态调整并查集大小
                if (lfd.size() * 2 > ufs.getSize()) {
                    ufs = UnionFindSet(lfd.size() * 2);
                } else {
                    ufs.init();
                }

                // 检测内部相交
                for (int i = 0; i < (int)lfd.size(); i++) {
                    Polygon* a = lfd[i];
                    for (int j = i + 1; j < (int)lfd.size(); j++) {
                        Polygon* b = lfd[j];
                        if (ufs.find(i) == ufs.find(j)) continue;
                        if (ManhattanIntersectDetector::manhattanPolygonsIntersect(a, b)) {
                            thread_local_edges.emplace_back(a->id, b->id);
                            ufs.join(i, j);
                        }
                    }
                }
            }

            // 线程处理完后将所有发现边一次性合并到结果
            if (!thread_local_edges.empty()) {
                std::lock_guard<std::mutex> lock(edges_mutex);
                 // 预分配空间避免重新分配
                if (edges.capacity() - edges.size() < thread_local_edges.size()) {
                    edges.reserve(edges.size() + thread_local_edges.size());
                }
                edges.insert(edges.end(),
                            std::make_move_iterator(thread_local_edges.begin()),
                            std::make_move_iterator(thread_local_edges.end()));
            }
        }
    }

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
            INFO_MSG( "Handling QuadTree : " << qtree->_name )

            qtree->GetAllLeafData(leafData, leafRect);

            // Process each leaf based on spatial bounds
            for (size_t i = 0; i < leafData.size(); ++i) {
                const auto& lfd = leafData[i];
                const auto& rect = leafRect[i];
                if (lfd.size() < 2) continue;

                INFO_INSTR(stats.total_leaf_nodes++;)
                INFO_INSTR(stats.total_polygon_pairs += (lfd.size() * (lfd.size() - 1)) / 2;)
                INFO_INSTR(stats.batch_grid_used++;)

                // Use batch grid detector
                size_t edges_nums_before = edges.size();
                bgid.batchManhattanPolygonsIntersect(rect, lfd, edges);
                INFO_INSTR(stats.intersections_found += (edges.size() - edges_nums_before);)
            }
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