#pragma once
#include "public.h"
#include "QuadTree.hpp"
#include "Graph.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <set>
#include <cstring>
#include <algorithm>
#include <utility>
#include <string_view>
#include <charconv>
#include <cstdint>
#include <cctype>
#ifdef __linux__
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

struct EdgeHash {
    size_t operator()(const Edge& e) const noexcept {
        return (static_cast<uint64_t>(e.first) << 32) ^
               static_cast<uint32_t>(e.second);
    }
};

// 数据块信息结构体，用于记录每个数据块的解析范围
struct ChunkInfo {
    const char* start = nullptr;   // 数据块起始位置指针
    const char* end = nullptr;     // 数据块结束位置指针
    int local_begin = 0;           // 数据块内起始多边形ID（局部编号,层内的偏移，最终生成任务需要在层的起始多边形id上+局部偏移）
    int local_count = 0;           // 数据块包含的多边形数量
};

// 层扫描信息结构体，用于记录每层数据的扫描范围和分块信息
struct LayerScanInfo {
    const char* start = nullptr;   // 该层数据起始位置指针
    const char* end = nullptr;     // 该层数据结束位置指针
    int polygon_count = 0;         // 该层包含的多边形总数
    std::vector<ChunkInfo> chunks; // 该层的数据块划分信息
};

// 解析任务范围结构体，定义并行解析的任务划分
struct ParseTaskRange {
    const char* start = nullptr;   // 任务数据起始位置指针
    const char* end = nullptr;     // 任务数据结束位置指针
    int begin_id = 0;              // 任务起始多边形ID（全局编号）
    int layer_id = 0;              // 任务所属的层ID
    int polygon_count = 0;         // 任务需要解析的多边形数量
};

// 布局文件视图类，提供对布局文件的高效内存映射访问
struct LayoutFileView {
    const char* data = nullptr;    // 文件数据指针
    size_t size = 0;               // 文件大小
    bool using_mmap = false;        // 标记是否使用内存映射方式
    std::vector<char> buffer;       // 文件数据缓冲区（当不使用内存映射时）
    
#ifdef __linux__
    void* mmap_ptr = MAP_FAILED;   // Linux下的内存映射指针
#endif

    // 默认构造函数
    LayoutFileView() = default;
    
    // 析构函数，自动释放资源
    ~LayoutFileView() { reset(); }
    
    // 禁止拷贝构造和拷贝赋值（因为管理资源）
    LayoutFileView(const LayoutFileView&) = delete;
    LayoutFileView& operator=(const LayoutFileView&) = delete;
    
    // 移动构造函数
    LayoutFileView(LayoutFileView&& other) noexcept { moveFrom(std::move(other)); }
    
    // 移动赋值运算符
    LayoutFileView& operator=(LayoutFileView&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(std::move(other));
        }
        return *this;
    }
    
    // 重置视图，释放所有资源
    void reset() {
#ifdef __linux__
        // Linux下解除内存映射
        if (using_mmap && mmap_ptr != MAP_FAILED) {
            munmap(mmap_ptr, size);
        }
        mmap_ptr = MAP_FAILED;
#endif
        using_mmap = false;
        data = nullptr;
        size = 0;
        buffer.clear();
    }

private:
    // 从另一个对象移动资源的辅助函数
    void moveFrom(LayoutFileView&& other) {
        using_mmap = other.using_mmap;
        size = other.size;
        buffer = std::move(other.buffer);
        
#ifdef __linux__
        mmap_ptr = other.mmap_ptr;
#endif
        
        // 根据访问方式设置数据指针
        if (using_mmap) {
#ifdef __linux__
            data = static_cast<const char*>(mmap_ptr);
#else
            data = nullptr;  // 非Linux平台不支持内存映射
#endif
        } else {
            data = buffer.empty() ? nullptr : buffer.data();
        }
        
        // 清空源对象状态
        other.using_mmap = false;
        other.size = 0;
        other.data = nullptr;
        other.buffer.clear();
#ifdef __linux__
        other.mmap_ptr = MAP_FAILED;
#endif
    }
};

class Input {
public:
    Rect layout;                                                    // 版图布局边界框
    robin_hood::unordered_map<std::string, int> layer_name_to_id;   // 图层名转换id, 0-index, 后续使用id索引
    robin_hood::unordered_map<int, std::string> layer_id_to_name;   // 图层id转名称，用于输出                                            

    int total_polygon = 0;                                          // 多边形总数
    std::vector<Polygon> polygons;                                  // 所有多边形的列表，下标对应多边形id, 0-index
    std::vector<Range> polygon_id_range_in_layer;                   // 每层的多边形id范围，双闭区间，下标对应图层id
    robin_hood::unordered_set<Edge, EdgeHash> via_rules;            // Via规则集合，表示哪些层可以通孔，通过排序保证id小的在前确保set不重复
    bool has_gate_rule;                                             // 是否存在Gate规则
    Edge gate_rule{};                                               // 元组表示Gate规则, gate_rule.first为Poly层，gate_rule.second为AA层
    std::vector<StartPos> start_pos;                                // 起始位置，可能多个，按输入顺序

private:
    Graph graph;                                                    // 图数据结构，用于表示版图数据层间的拓扑关系
    std::vector<LayerScanInfo> layer_scan_info_;                    // 图层扫描信息数组，记录每个图层的解析范围和多边形数量
    std::vector<int> layer_id_sequence_in_buffer;                   // 记录输入layout文件中 层layer_id 的出现顺序
    static constexpr size_t kDefaultVertexReserve = 12;             // 默认顶点预留数量，用于预分配内存以提高性能
    static constexpr size_t kCharsPerVertexEstimate = 18;           // 每个顶点预估的字符数，用于内存预分配估算
    static constexpr size_t kAaSplitReserveMultiplier = 7;          // 预估每个AA层多边形被切成7个
    static constexpr int kPreprocessChunkStride = 163840;
    // 十几层情况下，层数较少，二分查找可能比哈希更快
    std::vector<std::string> layer_names_cache_;                    // 图层名称缓存，存储所有图层的名称字符串
    struct LayerNameEntry {                                         // 图层名称条目结构，用于图层名称二分查找和排序
        std::string_view name;                                      // 图层名称的字符串视图
        int id;                                                     // 图层ID
    };
    std::vector<LayerNameEntry> layer_name_sorted_;                 // 排序后的图层名称列表，用于快速查找和二分搜索
    

public:
    // 根据文件路径初始化读取数据
    Input(const std::string& layout_path, const std::string& rule_path, int thread_count = 1) {
        layout = Rect(INT_MAX, INT_MAX, INT_MIN, INT_MIN);
        has_gate_rule = false;
        // 先读规则文件
        readRule(rule_path);
        // 按需读取版图文件
        if (thread_count == 1) {
            readLayout(layout_path);
        } else {
            readLayoutParallel(layout_path, thread_count);
        }
    }

    ~Input() = default;

    // 读取规则文件
    void readRule(const std::string& rule_path) {
        Timer timing;
        auto view = mapRuleFile(rule_path);
        const char* const data_begin = view.data;
        const char* const data_end = view.data ? view.data + view.size : view.data;
        enum class Section { None, StartPos, Via, Gate };
        Section section = Section::None;

        // 若有则暂存PO和AA层名字, 用于最后重排他们的id, 确保PO是倒数第二层id, AA是倒数第一层id, 方便合并与切割
        std::string po_name;
        std::string aa_name;
        int curr_layer_id = -1;

        auto ensureLayerId = [&](std::string_view name) -> int {    // 查看该层id是否出现过，未出现则新增
            std::string key(name);
            auto found = layer_name_to_id.find(key);
            if (found != layer_name_to_id.end()) {
                return found->second;
            }
            int new_id = ++curr_layer_id;
            auto inserted = layer_id_to_name.emplace(new_id, std::move(key));
            const std::string& stored = inserted.first->second;
            layer_name_to_id.emplace(stored, new_id);
            return new_id;
        };

        const char* cursor = data_begin;
        while (cursor && cursor < data_end) {
            const char* line_start = cursor;
            const char* newline = static_cast<const char*>(std::memchr(cursor, '\n', static_cast<size_t>(data_end - cursor)));
            if (!newline) {
                cursor = data_end;
            } else {
                cursor = newline + 1;
            }

            size_t line_len = (newline ? static_cast<size_t>(newline - line_start)
                                       : static_cast<size_t>(data_end - line_start));
            if (line_len > 0 && line_start[line_len - 1] == '\r') {
                --line_len;
            }

            std::string_view line(line_start, line_len);
            line = trimView(line);
            if (line.empty() || line.front() == '#') {
                continue;
            }

            if (line == "StartPos") {
                section = Section::StartPos;
                continue;
            }
            if (line == "Via") {
                section = Section::Via;
                continue;
            }
            if (line == "Gate") {
                section = Section::Gate;
                continue;
            }

            switch (section) {
            case Section::StartPos: {   // 起始位置
                std::string_view rest = line;
                std::string_view layer_token = consumeToken(rest);
                if (layer_token.empty()) {
                    break;
                }
                int layer_id = ensureLayerId(layer_token);

                rest = trimView(rest);
                if (rest.size() < 3 || rest.front() != '(' || rest.back() != ')') {
                    break;
                }
                rest.remove_prefix(1);
                rest.remove_suffix(1);

                size_t comma_pos = rest.find(',');
                if (comma_pos == std::string_view::npos) {
                    break;
                }

                std::string_view x_str = trimView(rest.substr(0, comma_pos));
                std::string_view y_str = trimView(rest.substr(comma_pos + 1));
                int x = 0;
                int y = 0;
                if (!parseInteger(x_str, x) || !parseInteger(y_str, y)) {
                    break;
                }
                start_pos.emplace_back(layer_id, Point{x, y});
                break;
            }
            case Section::Via: {    // 处理Via规则
                std::string_view content = line;
                std::string_view first_token = consumeToken(content);
                if (first_token.empty()) {
                    break;
                }
                int prev_id = ensureLayerId(first_token);
                while (true) {
                    std::string_view next_token = consumeToken(content);
                    if (next_token.empty()) {
                        break;
                    }
                    int curr_id = ensureLayerId(next_token);
                    if (prev_id != curr_id) {
                        int u = std::min(prev_id, curr_id);
                        int v = std::max(prev_id, curr_id);
                        via_rules.insert({u, v});
                    }
                    prev_id = curr_id;
                }
                break;
            }
            case Section::Gate: {   // 处理Gate规则
                std::string_view content = line;
                std::string_view poly_token = consumeToken(content);
                std::string_view aa_token = consumeToken(content);
                if (poly_token.empty() || aa_token.empty()) {
                    break;
                }
                has_gate_rule = true;
                po_name.assign(poly_token.begin(), poly_token.end());
                aa_name.assign(aa_token.begin(), aa_token.end());
                gate_rule.first = ensureLayerId(poly_token);
                gate_rule.second = ensureLayerId(aa_token);
                break;
            }
            case Section::None:
            default:
                break;
            }
        }

        view.reset();

        // 后处理：我们只处理必要的层, 即根据Via规则确定与起点所在层的层间联通分量, 只需保留这些层即可
        CreatLayerGraph();

        robin_hood::unordered_set<Edge, EdgeHash> filtered_via;
        /* 单起点多层/双起点多层：只关注起点所在层间连通分量的层 */
        if (!via_rules.empty() && !start_pos.empty()) {
            robin_hood::unordered_set<int> allowed_layers;
            // 获取起点有关的层联通分量
            for (const auto& st : start_pos) {
                auto component = GetConnectComponentofLayer(st.first);
                allowed_layers.insert(component.begin(), component.end());
            }
            // 过滤Via,只保留有效的Via
            for (const auto& via : via_rules) {
                if (allowed_layers.find(via.first) != allowed_layers.end() &&
                    allowed_layers.find(via.second) != allowed_layers.end()) {
                    filtered_via.insert(via);
                }
            }
        } else {    /* 单起点单层 (其实via_rules为空)*/
            filtered_via.insert(via_rules.begin(), via_rules.end());
        }

        via_rules.swap(filtered_via);

        // 遍历有效的Via, 收集层名
        std::set<std::string> valid_layers;
        for (auto& via : via_rules) {
            valid_layers.insert(layer_id_to_name[via.first]);
            valid_layers.insert(layer_id_to_name[via.second]);
        }
        for (auto& st : start_pos) {    // 起点层，防止遗漏
            valid_layers.insert(layer_id_to_name[st.first]);
        }
        if (has_gate_rule) {
            valid_layers.insert(po_name);
            valid_layers.insert(aa_name);
        }

        // 确定每层的最终id
        robin_hood::unordered_map<std::string, int> new_name_to_id;
        robin_hood::unordered_map<int, std::string> new_id_to_name;
        int remap_id = -1;
        for (const auto& layer_name : valid_layers) {
            if (has_gate_rule && (layer_name == po_name || layer_name == aa_name)) {
                continue;
            }
            new_name_to_id[layer_name] = ++remap_id;
            new_id_to_name[remap_id] = layer_name;
        }
        if (has_gate_rule) {    // PO与AA层放最后
            new_name_to_id[po_name] = ++remap_id;
            new_id_to_name[remap_id] = po_name;
            new_name_to_id[aa_name] = ++remap_id;
            new_id_to_name[remap_id] = aa_name;
        }

        // 更新起点层id信息
        for (auto& st : start_pos) {
            st.first = new_name_to_id[layer_id_to_name[st.first]];
        }

        // 更新Via_Rule层id信息
        robin_hood::unordered_set<Edge, EdgeHash> remapped_via;
        for (auto& via : via_rules) {
            int u = new_name_to_id[layer_id_to_name[via.first]];
            int v = new_name_to_id[layer_id_to_name[via.second]];
            if (u > v) std::swap(u, v);
            remapped_via.insert({u, v});
        }
        via_rules.swap(remapped_via);

        // 更新Gate_Rule层id信息
        if (has_gate_rule) {
            gate_rule.first = new_name_to_id[po_name];
            gate_rule.second = new_name_to_id[aa_name];
        }

        // 更新映射
        layer_name_to_id = std::move(new_name_to_id);
        layer_id_to_name = std::move(new_id_to_name);
        // 初始化每层的多边形id范围数组
        polygon_id_range_in_layer.assign(layer_name_to_id.size(), Range{0, -1});
        rebuildLayerNameCache();

        std::cout << "[Input][Timing] readRule total: " << timing.Elapsed() << " s" << std::endl;
    }

    // 读取版图文件
    void readLayout(const std::string& layout_path) {
        Timer stage;
        // 内存映射文件
        LayoutFileView layout_view = mapLayoutFile(layout_path);
        const char* file_data = layout_view.data;
        size_t file_size = layout_view.size;
        assert(file_data && file_size > 0 && "empty layout file");
        double buffer_time = stage.FromLastCallElapsed();

        // 第一轮扫描统计层信息
        size_t total_polygons_scanned = preprocessLayoutBuffer(file_data, file_size);
        double first_pass_time = stage.FromLastCallElapsed();

        // 预留多边形数组空间
        size_t reserve_count = total_polygons_scanned;
        if (has_gate_rule && gate_rule.second >= 0 && static_cast<size_t>(gate_rule.second) < layer_scan_info_.size()) {
            size_t aa_count = static_cast<size_t>(layer_scan_info_[gate_rule.second].polygon_count);
            reserve_count += aa_count * kAaSplitReserveMultiplier;  // 考虑AA层的切割数量
        }
        polygons.reserve(reserve_count);
        polygons.resize(total_polygons_scanned);
        double resize_time = stage.FromLastCallElapsed();

        // 更新层多边形的id范围
        size_t layer_count = layer_name_to_id.size();
        if (polygon_id_range_in_layer.size() < layer_count) {
            polygon_id_range_in_layer.resize(layer_count, Range{0, -1});
        }
        int next_polygon_id = 0;
        for (size_t layer_id = 0; layer_id < layer_count; ++layer_id) {
            auto& range = polygon_id_range_in_layer[layer_id];
            range.first = next_polygon_id;
            const int polygon_count = (layer_id < layer_scan_info_.size()) ? layer_scan_info_[layer_id].polygon_count : 0;
            range.second = next_polygon_id + polygon_count - 1;
            next_polygon_id += polygon_count;
        }
        total_polygon = next_polygon_id;
        double range_time = stage.FromLastCallElapsed();

        // 收集每个层分块的任务
        std::vector<ParseTaskRange> tasks;
        size_t total_chunks = 0;
        for (size_t layer_id = 0; layer_id < layer_count; ++layer_id) {
            const LayerScanInfo& info = (layer_id < layer_scan_info_.size()) ? layer_scan_info_[layer_id] : LayerScanInfo{};
            if (info.polygon_count <= 0) {
                continue;
            }
            total_chunks += info.chunks.size();
        }
        tasks.reserve(total_chunks);
        for (size_t layer_id = 0; layer_id < layer_count; ++layer_id) {
            const LayerScanInfo& info = (layer_id < layer_scan_info_.size()) ? layer_scan_info_[layer_id] : LayerScanInfo{};
            if (info.polygon_count <= 0) {
                continue;
            }
            int layer_begin = polygon_id_range_in_layer[layer_id].first;
            if (info.chunks.empty()) {
                tasks.push_back({info.start, info.end, layer_begin, static_cast<int>(layer_id), info.polygon_count});
            } else {
                for (const auto& chunk : info.chunks) {
                    if (chunk.local_count <= 0) {
                        continue;
                    }
                    const char* start_ptr = chunk.start ? chunk.start : info.start;
                    const char* end_ptr = chunk.end ? chunk.end : info.end;
                    if (!start_ptr || !end_ptr || start_ptr >= end_ptr) {
                        continue;
                    }
                    int begin_id = layer_begin + chunk.local_begin;
                    tasks.push_back({start_ptr, end_ptr, begin_id, static_cast<int>(layer_id), chunk.local_count});
                }
            }
        }
        double collect_task_time = stage.FromLastCallElapsed();

        // 依次执行解析任务
        for (size_t i = 0; i < tasks.size(); ++i) {
            executeParseRange(tasks[i], layout);
        }
        double parse_time = stage.FromLastCallElapsed();

        std::cout << "[Input][Timing] readLayout buffer: " << buffer_time
                  << " s, first_pass: " << first_pass_time
                  << " s, resize: " << resize_time
                  << " s, range: " << range_time
                  << " s, collect_task: " << collect_task_time
                  << " s, parse: " << parse_time
                  << " s, total: " << stage.Elapsed() << " s" << std::endl;
    }

#pragma region read_parallel
    // 并行读取版图文件
    void readLayoutParallel(const std::string& layout_path, int thread_count) {
        Timer stage;
        // 内存映射文件
        LayoutFileView layout_view = mapLayoutFile(layout_path);
        const char* file_data = layout_view.data;
        size_t file_size = layout_view.size;
        assert(file_data && file_size > 0 && "empty layout file");
        double buffer_time = stage.FromLastCallElapsed();

        // 第一轮扫描统计层信息
        size_t total_polygons_scanned = preprocessLayoutBuffer(file_data, file_size);
        double first_pass_time = stage.FromLastCallElapsed();

        // 预留多边形数组空间
        size_t reserve_count = total_polygons_scanned;
        if (has_gate_rule && gate_rule.second >= 0 && static_cast<size_t>(gate_rule.second) < layer_scan_info_.size()) {
            size_t aa_count = static_cast<size_t>(layer_scan_info_[gate_rule.second].polygon_count);
            reserve_count += aa_count * kAaSplitReserveMultiplier;  // 考虑AA层的切割数量
        }
        polygons.reserve(reserve_count);
        polygons.resize(total_polygons_scanned);
        double resize_time = stage.FromLastCallElapsed();

        // 更新层多边形的id范围
        size_t layer_count = layer_name_to_id.size();
        if (polygon_id_range_in_layer.size() < layer_count) {
            polygon_id_range_in_layer.resize(layer_count, Range{0, -1});
        }
        int next_polygon_id = 0;
        for (size_t layer_id = 0; layer_id < layer_count; ++layer_id) {
            auto& range = polygon_id_range_in_layer[layer_id];
            range.first = next_polygon_id;
            const int polygon_count = (layer_id < layer_scan_info_.size()) ? layer_scan_info_[layer_id].polygon_count : 0;
            range.second = next_polygon_id + polygon_count - 1;
            next_polygon_id += polygon_count;
        }
        total_polygon = next_polygon_id;
        double range_time = stage.FromLastCallElapsed();

        // 收集每个层分块的任务
        std::vector<ParseTaskRange> tasks;
        size_t total_chunks = 0;
        for (size_t layer_id = 0; layer_id < layer_count; ++layer_id) {
            const LayerScanInfo& info = (layer_id < layer_scan_info_.size()) ? layer_scan_info_[layer_id] : LayerScanInfo{};
            if (info.polygon_count <= 0) {
                continue;
            }
            total_chunks += info.chunks.size();
        }
        tasks.reserve(total_chunks);
        for (size_t layer_id = 0; layer_id < layer_count; ++layer_id) {
            const LayerScanInfo& info = (layer_id < layer_scan_info_.size()) ? layer_scan_info_[layer_id] : LayerScanInfo{};
            if (info.polygon_count <= 0) {
                continue;
            }
            int layer_begin = polygon_id_range_in_layer[layer_id].first;
            if (info.chunks.empty()) {
                tasks.push_back({info.start, info.end, layer_begin, static_cast<int>(layer_id), info.polygon_count});
            } else {
                for (const auto& chunk : info.chunks) {
                    if (chunk.local_count <= 0) {
                        continue;
                    }
                    const char* start_ptr = chunk.start ? chunk.start : info.start;
                    const char* end_ptr = chunk.end ? chunk.end : info.end;
                    if (!start_ptr || !end_ptr || start_ptr >= end_ptr) {
                        continue;
                    }
                    int begin_id = layer_begin + chunk.local_begin;
                    tasks.push_back({start_ptr, end_ptr, begin_id, static_cast<int>(layer_id), chunk.local_count});
                }
            }
        }
        double collect_task_time = stage.FromLastCallElapsed();

        // 按任务量从大到小排序任务
        std::vector<Rect> thread_local_layouts(thread_count);
        std::sort(tasks.begin(), tasks.end(), [](const ParseTaskRange& a, const ParseTaskRange& b) {
            return a.polygon_count > b.polygon_count;
        });
        double sort_task_time = stage.FromLastCallElapsed();

        // 任务并行解析
        #pragma omp parallel for schedule(dynamic) num_threads(thread_count)
        for (size_t i = 0; i < tasks.size(); ++i) {
            int tid = omp_get_thread_num();
            Rect& local_rect = thread_local_layouts[tid];
            executeParseRange(tasks[i], local_rect);
        }
        for (auto& rect : thread_local_layouts) {
            if (rect._xmin != INT_MAX) {
                layout.update(rect);
            }
        }
        double parse_time = stage.FromLastCallElapsed();

        std::cout << "[Input][Timing] readLayoutParallel buffer: " << buffer_time
                  << " s, first_pass: " << first_pass_time
                  << " s, resize: " << resize_time
                  << " s, range: " << range_time
                  << " s, collect_task: " << collect_task_time
                  << " s, sort_task: " << sort_task_time
                  << " s, parse: " << parse_time
                  << " s, total: " << stage.Elapsed() << " s" << std::endl;
    }
#pragma endregion

    // 打印版图信息
    void PrintLayoutInfo() {
        std::cout << "[Layout]" << std::endl;
        std::cout << "total polygon num:" << total_polygon << std::endl;
        std::cout << "total layer num:" << polygon_id_range_in_layer.size() << std::endl;
        for (auto& name_id : layer_name_to_id) {
            const auto& range = polygon_id_range_in_layer[name_id.second];
            int polygon_num = (range.second >= range.first) ? (range.second - range.first + 1) : 0;
            std::cout << "layer name:" << name_id.first << " id:" << name_id.second
                      << " polygon num:" << polygon_num
                      << " polygon id range:[" << range.first << "," << range.second << "]" << std::endl;
        }
    }

    // 打印规则信息
    void PrintRuleInfo() {
        std::cout << "[Rule]" << std::endl;
        for (auto& st : start_pos) {
            std::cout << "StartPos:" << layer_id_to_name[st.first] << " " << st.second.first << " " << st.second.second << std::endl;
        }
        for (auto& vi : via_rules) {
            std::cout << "Via:" << layer_id_to_name[vi.first] << " " << layer_id_to_name[vi.second] << std::endl;
        }
        if (has_gate_rule) {
            std::cout << "Gate:" << layer_id_to_name[gate_rule.first] << " " << layer_id_to_name[gate_rule.second] << std::endl;
        } else {
            std::cout << "no Gate" << std::endl;
        }
    }

    // 获取多边形的矩形包围盒
    Rect GetRectofPolygon(Vertexs& poly_vertex) {
        auto vertex_iter = poly_vertex.begin();
        auto vertex_end = poly_vertex.end();
        const int first_x = vertex_iter->x;
        const int first_y = vertex_iter->y;
        int xmin = first_x;
        int xmax = first_x;
        int ymin = first_y;
        int ymax = first_y;
        ++vertex_iter;
        for (; vertex_iter != vertex_end; ++vertex_iter) {
            int x = vertex_iter->x;
            int y = vertex_iter->y;
            xmin = std::min(xmin, x);
            xmax = std::max(xmax, x);
            ymin = std::min(ymin, y);
            ymax = std::max(ymax, y);
        }
        return Rect(xmin, ymin, xmax, ymax);
    }

private:

    LayoutFileView mapFileCommon(const std::string& path) {
        LayoutFileView view;
#ifdef __linux__
        // Linux平台使用内存映射文件提高大文件读取性能
        int fd = ::open(path.c_str(), O_RDONLY);
        if (fd >= 0) {
            struct stat st {};
            if (fstat(fd, &st) == 0 && st.st_size > 0) {
                size_t file_size = static_cast<size_t>(st.st_size);
                void* addr = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
                if (addr != MAP_FAILED) {
                    // 优化内存访问模式：顺序访问+预读取提示
                    posix_madvise(addr, file_size, POSIX_MADV_SEQUENTIAL);
                    posix_madvise(addr, file_size, POSIX_MADV_WILLNEED);

                    view.using_mmap = true;
                    view.size = file_size;
                    view.data = static_cast<const char*>(addr);
                    view.mmap_ptr = addr;
                    ::close(fd);
                    return view;
                }
            }
            ::close(fd);
        }
#endif
        // 非Linux平台或内存映射失败时使用传统文件读取方式
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        assert(file.is_open() && "failed to open file");
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        if (size > 0) {
            view.size = static_cast<size_t>(size);
            view.buffer.resize(view.size);
            file.read(view.buffer.data(), size);
            assert(file.gcount() == size && "failed to read file");
            view.data = view.buffer.data();
        }
        return view;
    }

    LayoutFileView mapLayoutFile(const std::string& layout_path) {
        return mapFileCommon(layout_path);  // 映射版图文件
    }

    LayoutFileView mapRuleFile(const std::string& rule_path) {
        return mapFileCommon(rule_path);    // 映射规则文件
    }

    /* 字符串处理工具函数 */
    static std::string_view trimLeft(std::string_view sv) {     // 去除左侧空白字符
        size_t idx = 0;
        while (idx < sv.size() && std::isspace(static_cast<unsigned char>(sv[idx]))) {
            ++idx;
        }
        sv.remove_prefix(idx);
        return sv;
    }
    static std::string_view trimRight(std::string_view sv) {    // 去除右侧空白字符
        size_t idx = sv.size();
        while (idx > 0 && std::isspace(static_cast<unsigned char>(sv[idx - 1]))) {
            --idx;
        }
        sv.remove_suffix(sv.size() - idx);
        return sv;
    }
    static std::string_view trimView(std::string_view sv) {     // 去除两侧空白字符
        return trimRight(trimLeft(sv));
    }
    static std::string_view consumeToken(std::string_view& sv) {    // 从字符串视图中提取一个token
        sv = trimLeft(sv);
        if (sv.empty()) {
            return {};
        }
        size_t pos = 0;
        while (pos < sv.size() && !std::isspace(static_cast<unsigned char>(sv[pos]))) {
            ++pos;
        }
        std::string_view token = sv.substr(0, pos);
        sv.remove_prefix(pos);
        return token;
    }
    static bool parseInteger(std::string_view sv, int& value) {     // 解析整数字符串
        sv = trimView(sv);
        if (sv.empty()) {
            return false;
        }
        int parsed = 0;
        auto result = std::from_chars(sv.data(), sv.data() + sv.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != sv.data() + sv.size()) {
            return false;
        }
        value = parsed;
        return true;
    }

    // 重建图层名称缓存，优化查找性能
    void rebuildLayerNameCache() {
        size_t count = layer_name_to_id.size();
        layer_names_cache_.assign(count, std::string{});
        for (const auto& kv : layer_id_to_name) {
            if (kv.first >= 0 && static_cast<size_t>(kv.first) < count) {
                layer_names_cache_[kv.first] = kv.second;
            }
        }
        layer_name_sorted_.clear();
        layer_name_sorted_.reserve(layer_names_cache_.size());
        for (size_t idx = 0; idx < layer_names_cache_.size(); ++idx) {
            const std::string& name = layer_names_cache_[idx];
            if (!name.empty()) {
                layer_name_sorted_.push_back(LayerNameEntry{std::string_view{name}, static_cast<int>(idx)});
            }
        }
        // 按名称排序以便二分查找
        std::sort(layer_name_sorted_.begin(), layer_name_sorted_.end(),
            [](const LayerNameEntry& lhs, const LayerNameEntry& rhs) {
                return lhs.name < rhs.name;
            });
    }

    // 通过图层名称查找图层ID
    int lookupLayerId(const char* name_begin, size_t name_len) const {
        std::string_view target(name_begin, name_len);
        target = trimView(target);
        if (target.empty()) {
            return -1;
        }
        // 使用二分查找在排序的图层名称列表中查找
        auto it = std::lower_bound(
            layer_name_sorted_.begin(), layer_name_sorted_.end(), target,
            [](const LayerNameEntry& entry, std::string_view value) {
                return entry.name < value;
            });
        if (it != layer_name_sorted_.end() && it->name == target) {
            return it->id;
        }
        return -1;  // 未找到返回-1
    }

    // 创建图层连通关系图
    void CreatLayerGraph() {
        graph = Graph(static_cast<int>(layer_name_to_id.size()));
        std::vector<Edge> edges;
        edges.reserve(via_rules.size());
        for (auto& via : via_rules) {
            edges.push_back(via);
        }
        graph.AddEdges(edges);
    }

    // 获取图层的连通分量
    robin_hood::unordered_set<int> GetConnectComponentofLayer(int layer_id) const {
        std::vector<int> component = graph.GetConnectedComponent(layer_id);
        robin_hood::unordered_set<int> res;
        for (auto& node : component) {
            res.insert(node);
        }
        return res;
    }

    // 快速解析顶点坐标
    bool fastParseCoordinates(const char* line_start, const char* line_end, Vertexs& vertex, Rect& out_rect) const {
        const char* ptr = line_start;
        const char* end = line_end;
        bool has_point = false;

        while (ptr < end) {
            while (ptr < end && *ptr != '(') {
                ++ptr;
            }
            if (ptr >= end) break;
            ++ptr; // skip '('

            bool x_negative = false;
            if (ptr < end && *ptr == '-') {
                x_negative = true;
                ++ptr;
            }
            int x = 0;
            while (ptr < end && *ptr >= '0' && *ptr <= '9') {
                x = x * 10 + (*ptr - '0');
                ++ptr;
            }
            if (x_negative) {
                x = -x;
            }

            if (ptr >= end || *ptr != ',') {
                break;
            }
            ++ptr; // skip ','

            bool y_negative = false;
            if (ptr < end && *ptr == '-') {
                y_negative = true;
                ++ptr;
            }
            int y = 0;
            while (ptr < end && *ptr >= '0' && *ptr <= '9') {
                y = y * 10 + (*ptr - '0');
                ++ptr;
            }
            if (y_negative) {
                y = -y;
            }

            if (ptr >= end || *ptr != ')') {
                break;
            }
            ++ptr; // skip ')'
            if (ptr < end && *ptr == ',') {
                ++ptr;
            }

            vertex.emplace_back(x, y);
            if (!has_point) {
                out_rect = Rect(x, y, x, y);
                has_point = true;
            } else {
                out_rect._xmin = std::min(out_rect._xmin, x);
                out_rect._xmax = std::max(out_rect._xmax, x);
                out_rect._ymin = std::min(out_rect._ymin, y);
                out_rect._ymax = std::max(out_rect._ymax, y);
            }
        }

        return has_point;
    }

    // 预估顶点数
    void preparePolygonVertices(Polygon& polygon, const char* line_start, size_t line_len) const {
        Vertexs& verts = polygon.vertex;
        size_t estimate = std::max<size_t>(kDefaultVertexReserve, line_len / kCharsPerVertexEstimate);
        verts.reserve(estimate);
    }

    // 第一轮扫描, 统计每层的信息+解析任务分块
    size_t preprocessLayoutBuffer(const char* data_begin, size_t file_size) {
        // 初始化层扫描信息数组
        size_t layer_count = layer_name_to_id.size();
        layer_scan_info_.assign(layer_count, LayerScanInfo{});

        // 当前处理的层信息指针和分块状态变量
        LayerScanInfo* current_layer = nullptr;
        const char* chunk_start_ptr = nullptr;
        int chunk_local_first = 0;
        int chunk_local_count = 0;
        const char* ptr = data_begin;
        const char* end = data_begin + file_size;
        size_t total_polygons = 0;

        // 完成构建一个任务分块函数
        auto finalize_chunk = [&](LayerScanInfo* layer, const char* chunk_start, const char* chunk_end, int local_first, int local_count) {
            if (!layer || local_count <= 0 || !chunk_start || !chunk_end) {
                return;
            }
            ChunkInfo info;
            info.start = chunk_start;
            info.end = chunk_end;
            info.local_begin = local_first;
            info.local_count = local_count;
            layer->chunks.push_back(info);
        };

        // 逐行扫描数据缓冲区
        while (ptr < end) {
            // 提取当前行（从ptr到下一个换行符或文件末尾）
            const char* line_start = ptr;
            const char* line_break = static_cast<const char*>(std::memchr(ptr, '\n', static_cast<size_t>(end - ptr)));
            if (!line_break) {
                ptr = end;  // 没有换行符，到达文件末尾
            } else {
                ptr = line_break + 1;   // 移动到下一行开始
            }

            // 计算行结束位置和长度，处理Windows换行符\r\n
            const char* line_end = line_break ? line_break : end;
            size_t line_len = static_cast<size_t>(line_end - line_start);
            if (line_len > 0 && line_start[line_len - 1] == '\r') {
                --line_len; // 去除\r字符
            }
            // 跳过连续的空行
            while (ptr < end && (*ptr == '\n' || *ptr == '\r')) {
                ++ptr;
            }

            // 处理空行：设置当前层的结束位置
            if (line_len == 0) {
                if (current_layer && current_layer->polygon_count > 0 && current_layer->end == nullptr) {
                    current_layer->end = line_end;
                }
                continue;
            }

            // 检查是否为新层标识行（不以'('开头的行）
            const char first_char = *line_start;
            if (first_char != '(') {
                // 结束前一层的扫描（如果有活跃的层）
                if (current_layer && current_layer->polygon_count > 0 && current_layer->end == nullptr) {
                    current_layer->end = line_start;
                    finalize_chunk(current_layer, chunk_start_ptr, line_start, chunk_local_first, chunk_local_count);
                }

                // 解析新层名，查找层ID
                int layer_idx = lookupLayerId(line_start, line_len);
                if (layer_idx < 0) {
                    // 不是目标层，跳过该层数据
                    current_layer = nullptr;
                    chunk_start_ptr = nullptr;
                    chunk_local_first = 0;
                    chunk_local_count = 0;
                    continue;
                }

                // 切换到新层，初始化层信息
                current_layer = &layer_scan_info_[layer_idx];
                current_layer->start = nullptr;
                current_layer->end = nullptr;
                current_layer->polygon_count = 0;
                current_layer->chunks.clear();
                chunk_start_ptr = nullptr;
                chunk_local_first = 0;
                chunk_local_count = 0;
                continue;
            }

            // 如果不是当前目标层的数据，跳过
            if (!current_layer) {
                continue;
            }

            // 初始化层的起始位置（如果是层的第一个多边形）
            if (current_layer->polygon_count == 0) {
                current_layer->start = line_start;
                chunk_start_ptr = line_start;
                chunk_local_first = 0;
            }

            // 更新层信息：增加多边形计数，更新结束位置
            current_layer->polygon_count++;
            ++total_polygons;

            // 初始化分块起始位置（如果分块还未开始）
            if (chunk_start_ptr == nullptr) {
                chunk_start_ptr = line_start;
                chunk_local_first = current_layer->polygon_count - 1;
                chunk_local_count = 0;
            }
            ++chunk_local_count;

            // 多边形数达到分块大小，完成一个分块
            if (chunk_local_count >= kPreprocessChunkStride) {
                finalize_chunk(current_layer, chunk_start_ptr, ptr, chunk_local_first, chunk_local_count);
                // 重置分块状态，准备下一个分块
                chunk_start_ptr = ptr;
                chunk_local_first = current_layer->polygon_count;
                chunk_local_count = 0;
            }
        }

        // 主循环结束后，处理最后一个未完成的分块
        if (current_layer && chunk_local_count > 0) {
            finalize_chunk(current_layer, chunk_start_ptr, current_layer->end ? current_layer->end : end, chunk_local_first, chunk_local_count);
            chunk_local_count = 0;
            chunk_start_ptr = nullptr;
        }
        // 确保当前层的结束位置被正确设置
        if (current_layer && current_layer->polygon_count > 0 && current_layer->end == nullptr) {
            current_layer->end = end;
        }
       
        return total_polygons;
    }

    // 处理一个多边形解析任务
    void executeParseRange(const ParseTaskRange& task, Rect& local_layout) {
        const char* current = task.start;
        const char* end_ptr = task.end;
        int curr_id = task.begin_id;

        while (current < end_ptr) {
            const char* line_start = current;
            while (current < end_ptr && *current != '\n' && *current != '\r') {
                ++current;
            }
            size_t line_len = current - line_start;
            if (line_len > 0 && *line_start == '(') {
                if (curr_id >= static_cast<int>(polygons.size())) {
                    break;
                }
                Polygon& poly = polygons[curr_id];
                poly.id = curr_id;
                poly.layer_id = task.layer_id;
				poly.vertex.reserve(12);
                // preparePolygonVertices(poly, line_start, line_len);
                Rect poly_rect;
                if (fastParseCoordinates(line_start, line_start + line_len, poly.vertex, poly_rect)) {
                    poly.rect = poly_rect;
                    local_layout.update(poly_rect);
                }
                ++curr_id;
            }
            while (current < end_ptr && (*current == '\n' || *current == '\r')) {
                ++current;
            }
        }
    }
};
