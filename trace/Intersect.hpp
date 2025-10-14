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
    PARALLEL                    // Parallel processing
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
    mutable IntersectionStats stats;

    // Batch grid intersection detector
    BatchGridIntersectDetector bgid;

    // Buffer for collecting leaf nodes
    std::vector<QuadTreeNode*> leaf_buffer;

    // Cache for neighbor candidate detection
    robin_hood::unordered_set<int> neighbor_candidate_cache;
    std::vector<int> dsu_parent;
    std::vector<uint8_t> dsu_rank;

    // Statistics for lazy neighbor detection
    size_t lazy_neighbor_calls = 0;
    size_t lazy_neighbor_candidates = 0;
    size_t lazy_neighbor_enqueues = 0;
    size_t lazy_neighbor_cache_hits = 0;
    size_t lazy_neighbor_duplicates = 0;

    /**
     * @brief Structure for caching sweep-line indexed data for leaf nodes
     */
    struct LeafSweepIndex {
        bool built = false;
        std::vector<int> polygon_ids;
        robin_hood::unordered_map<int, size_t> id_to_local;
        std::vector<std::vector<int>> adjacency;
    };

    // Cache for sweep-line indexed leaf data
    robin_hood::unordered_map<const QuadTreeNode*, LeafSweepIndex> leaf_sweep_cache;

    /**
     * @brief Statistics for leaf node processing
     */
    struct LeafStats {
        size_t visits = 0;
        size_t candidate_checks = 0;
        size_t true_hits = 0;
        size_t leaf_size = 0;
    };

    // Statistics for leaf nodes
    robin_hood::unordered_map<const QuadTreeNode*, LeafStats> leaf_stats;

    // Threshold for considering a leaf as "heavy"
    static constexpr size_t HEAVY_LEAF_THRESHOLD = 256;

    // Current intersection detection method
    IntersectionMethod method = IntersectionMethod::MANHATTAN_COMPLETE;

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
    ~Intersect() { FlushStats(); }

    /**
     * @brief Flushes and prints statistics
     */
    void FlushStats() {
        if (lazy_neighbor_calls > 0) {
            std::cout << "[LazyBFS] neighbor stats - calls=" << lazy_neighbor_calls
                << " candidates=" << lazy_neighbor_candidates
                << " enqueued=" << lazy_neighbor_enqueues
                << " cache_hits=" << lazy_neighbor_cache_hits
                << " duplicates=" << lazy_neighbor_duplicates << std::endl;
        }

        if (leaf_stats.empty()) {
            return;
        }

        // Sort and display top heavy leaves
        std::vector<const LeafStats*> stats_vec;
        stats_vec.reserve(leaf_stats.size());
        for (auto& kv : leaf_stats) {
            stats_vec.push_back(&kv.second);
        }
        std::sort(stats_vec.begin(), stats_vec.end(), [](const LeafStats* a, const LeafStats* b) {
            return a->visits > b->visits;
            });

        const size_t limit = std::min<size_t>(stats_vec.size(), 10);
        if (limit == 0) {
            return;
        }

        std::cout << "[Leaf Stats] top " << limit << " heavy leaves" << std::endl;
        for (size_t i = 0; i < limit; ++i) {
            const LeafStats& stat = *stats_vec[i];
            std::cout << "  visits=" << stat.visits
                << " candidates=" << stat.candidate_checks
                << " hits=" << stat.true_hits
                << " leaf_size=" << stat.leaf_size << std::endl;
        }
    }

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
        case IntersectionMethod::BATCH_GRID:
            processWithBatchGrid(quad_trees, edges);
            break;
        }

        // Remove duplicate edges
        removeDuplicateEdges(edges);

        // Print statistics
        stats.total_time_ms = total_timer.ElapsedMs();
        stats.print();

        return edges;
    }

    /**
     * @brief Sets the intersection detection method
     * @param new_method The method to use
     */
    void setIntersectionMethod(IntersectionMethod new_method) {
        method = new_method;
        std::cout << "Switched to method: " << getMethodName() << std::endl;
    }

    /**
     * @brief Gets the name of the current method
     * @return Method name string
     */
    std::string getMethodName() const {
        switch (method) {
        case IntersectionMethod::MANHATTAN_COMPLETE: return "Manhattan Complete Detection";
        case IntersectionMethod::BATCH_GRID: return "Batch Grid";
        case IntersectionMethod::PARALLEL: return "Parallel";
        default: return "Unknown";
        }
    }

    /**
     * @brief Gets intersection statistics
     * @return Reference to statistics object
     */
    const IntersectionStats& getStats() const { return stats; }

    /**
     * @brief Gets neighboring polygons that intersect with the given polygon using lazy evaluation
     * @param polygon_id ID of the source polygon
     * @param visit_token Visit tracking array
     * @param visit_version Current visit version
     * @param neighbors Output vector for neighbor IDs
     */
    void GetNeighborsLazy(int polygon_id, const std::vector<int>& visit_token, int visit_version, std::vector<int>& neighbors) {
        neighbors.clear();

        if (polygon_id < 0 || polygon_id >= static_cast<int>(input.polygons.size())) {
            return;
        }

        Polygon* source = input.polygons[polygon_id];
        neighbor_candidate_cache.clear();
        ++lazy_neighbor_calls;

        const auto& quad_trees = spaceIndex.GetQuadTreesForLayer(source->layer_id);

        for (auto* qtree : quad_trees) {
            if (!qtree) {
                continue;
            }

            // Collect leaves that intersect with the source polygon's bounding box
            leaf_buffer.clear();
            qtree->CollectIntersectLeaves(source->rect, leaf_buffer);

            for (auto* leaf : leaf_buffer) {
                if (!leaf) {
                    continue;
                }

                const auto leaf_size = leaf->_datas.size();
                if (leaf_size == 0) {
                    continue;
                }

                // Use cached sweep index for heavy leaves, direct checking for light leaves
                if (leaf_size > HEAVY_LEAF_THRESHOLD) {
                    auto& sweep = ensureLeafSweepIndex(leaf);
                    LeafStats& stat = leaf_stats[leaf];
                    stat.visits++;

                    auto local_it = sweep.id_to_local.find(polygon_id);
                    if (local_it != sweep.id_to_local.end()) {
                        const auto& adjacency_list = sweep.adjacency[local_it->second];
                        const size_t candidate_count = adjacency_list.size();
                        stat.candidate_checks += candidate_count;
                        lazy_neighbor_cache_hits += candidate_count;
                        lazy_neighbor_candidates += candidate_count;

                        for (int candidate_id : adjacency_list) {
                            if (visit_token[candidate_id] == visit_version) {
                                continue;
                            }

                            if (!neighbor_candidate_cache.insert(candidate_id).second) {
                                ++lazy_neighbor_duplicates;
                                continue;
                            }

                            neighbors.push_back(candidate_id);
                            ++lazy_neighbor_enqueues;
                        }
                    }
                }
                else {
                    // Direct checking for light leaves
                    for (auto* candidate : leaf->_datas) {
                        if (!candidate) {
                            continue;
                        }

                        ++lazy_neighbor_candidates;
                        const int candidate_id = candidate->id;

                        if (candidate_id == polygon_id) {
                            continue;
                        }

                        if (visit_token[candidate_id] == visit_version) {
                            continue;
                        }

                        if (!neighbor_candidate_cache.insert(candidate_id).second) {
                            ++lazy_neighbor_duplicates;
                            continue;
                        }

                        // Quick bounding box check
                        if (!source->rect.Intersects(candidate->rect)) {
                            continue;
                        }

                        // Detailed Manhattan intersection check
                        if (ManhattanIntersectDetector::manhattanPolygonsIntersect(source, candidate)) {
                            neighbors.push_back(candidate_id);
                            ++lazy_neighbor_enqueues;
                        }
                    }
                }
            }
        }
    }

private:
    /**
     * @brief Ensures sweep index is built for a leaf node
     * @param leaf The leaf node
     * @return Reference to the leaf's sweep index
     */
    LeafSweepIndex& ensureLeafSweepIndex(QuadTreeNode* leaf) {
        LeafStats& stat = leaf_stats[leaf];
        stat.leaf_size = leaf->_datas.size();

        auto& entry = leaf_sweep_cache[leaf];

        if (!entry.built) {
            const size_t n = leaf->_datas.size();
            entry.polygon_ids.resize(n);
            entry.id_to_local.clear();
            entry.id_to_local.reserve(n * 2);
            entry.adjacency.clear();
            entry.adjacency.resize(n);

            // Build polygon ID mapping
            for (size_t i = 0; i < n; ++i) {
                int pid = leaf->_datas[i]->id;
                entry.polygon_ids[i] = pid;
                entry.id_to_local.emplace(pid, i);
            }

            // Sort polygons by x-min for sweep line algorithm
            std::vector<size_t> indices(n);
            for (size_t i = 0; i < n; ++i) indices[i] = i;
            std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
                return leaf->_datas[a]->rect._xmin < leaf->_datas[b]->rect._xmin;
                });

            // Sweep line algorithm to find intersections
            std::vector<size_t> active;
            active.reserve(n);

            for (size_t idx : indices) {
                Polygon* poly = leaf->_datas[idx];
                int xmin = poly->rect._xmin;
                int ymin = poly->rect._ymin;
                int ymax = poly->rect._ymax;

                // Remove polygons that are no longer active
                active.erase(std::remove_if(active.begin(), active.end(), [&](size_t other_idx) {
                    return leaf->_datas[other_idx]->rect._xmax < xmin;
                    }), active.end());

                // Check intersections with active polygons
                for (size_t other_idx : active) {
                    Polygon* other = leaf->_datas[other_idx];
                    if (other->rect._ymax < ymin || other->rect._ymin > ymax) continue;

                    stat.candidate_checks++;
                    if (!ManhattanIntersectDetector::manhattanPolygonsIntersect(poly, other)) continue;

                    // Record intersection in adjacency list
                    size_t local_a = entry.id_to_local[poly->id];
                    size_t local_b = entry.id_to_local[other->id];
                    entry.adjacency[local_a].push_back(other->id);
                    entry.adjacency[local_b].push_back(poly->id);
                    stat.true_hits++;
                }

                active.push_back(idx);
            }

            entry.built = true;
        }

        return entry;
    }

    // ========== Intersection Detection Method Implementations ==========

    /**
     * @brief Process using complete Manhattan edge intersection detection
     * @param quad_trees Vector of quad trees to process
     * @param edges Output vector for intersecting edges
     */
    void processWithManhattanComplete(const std::vector<QuadTree*>& quad_trees, std::vector<std::pair<int, int>>& edges) {
        std::vector<std::vector<Polygon*>> leafData;
        leafData.reserve(1000000);

        for (auto& qtree : quad_trees) {
            leafData.clear();
            std::cout << "Handling QuadTree : " << qtree->_name << std::endl;
            qtree->GetAllLeafData(leafData);

            for (const auto& lfd : leafData) {
                const size_t local_size = lfd.size();
                if (local_size < 2) {
                    continue;
                }

                stats.total_leaf_nodes++;
                stats.total_polygon_pairs += (local_size * (local_size - 1)) / 2;
                stats.manhattan_complete_used++;

                // Initialize DSU for connected components
                if (dsu_parent.size() < local_size) {
                    dsu_parent.resize(local_size);
                    dsu_rank.resize(local_size);
                }

                std::iota(dsu_parent.begin(), dsu_parent.begin() + local_size, 0);
                std::fill_n(dsu_rank.begin(), local_size, static_cast<uint8_t>(0));

                auto find_root = [&](int x) {
                    while (dsu_parent[x] != x) {
                        dsu_parent[x] = dsu_parent[dsu_parent[x]];
                        x = dsu_parent[x];
                    }
                    return x;
                    };

                auto unite = [&](int a, int b) {
                    int ra = find_root(a);
                    int rb = find_root(b);
                    if (ra == rb) {
                        return false;
                    }
                    if (dsu_rank[ra] < dsu_rank[rb]) {
                        std::swap(ra, rb);
                    }
                    dsu_parent[rb] = ra;
                    if (dsu_rank[ra] == dsu_rank[rb]) {
                        ++dsu_rank[ra];
                    }
                    return true;
                    };

                // Check all polygon pairs in the leaf
                for (int first_idx = 0; first_idx < static_cast<int>(local_size); ++first_idx) {
                    Polygon* a = lfd[first_idx];
                    for (int second_idx = first_idx + 1; second_idx < static_cast<int>(local_size); ++second_idx) {
                        if (find_root(first_idx) == find_root(second_idx)) {
                            continue;
                        }

                        Polygon* b = lfd[second_idx];
                        if (ManhattanIntersectDetector::manhattanPolygonsIntersect(a, b)) {
                            edges.emplace_back(a->id, b->id);
                            unite(first_idx, second_idx);
                            stats.intersections_found++;
                        }
                    }
                }
            }
        }
    }

    /**
     * @brief Process using batch grid method
     * @param quad_trees Vector of quad trees to process
     * @param edges Output vector for intersecting edges
     */
    void processWithBatchGrid(const std::vector<QuadTree*>& quad_trees, std::vector<std::pair<int, int>>& edges) {
        std::vector<std::vector<Polygon*>> leafData;
        std::vector<Rect> leafRect;
        leafData.reserve(1000000);
        leafRect.reserve(1000000);

        for (auto& qtree : quad_trees) {
            // Collect spatial data
            leafData.clear();
            leafRect.clear();
            std::cout << "Handling QuadTree : " << qtree->_name << std::endl;
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