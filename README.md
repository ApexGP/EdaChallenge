# 2025 中国研究生创“芯”大赛·EDA 精英挑战赛
## 赛题三：版图多层链路追踪算法

赛题描述：[EDA赛题三](./doc/eda赛题三.pdf)

## 导航

- [1. 问题分析](#1-问题分析)
- [2. 目录结构](#2-目录结构)
- [3. 代码结构](#3-代码结构)
- [4. 编译](#4-编译)
- [5. 运行](#5-运行)
- [6. 性能分析工具](#6-性能分析工具)
- [7. 代码改动记录](#7-代码改动记录)
- [8. 性能记录](#8-性能记录)

## 1. 问题分析

本项目解决的是版图多层链路追踪问题。本质上，可以将版图中的多边形视为图节点，将同层或跨层相交关系视为边，然后从规则文件给定的起点出发求电学连通分量。

主要难点：

- 海量曼哈顿多边形的存储、解析和过滤。
- 多边形相交检测的空间索引与快速判定。
- Via 相邻层连接关系的建模。
- Gate 规则下 PO 层合并与 AA 层切割。
- 单线程性能和多线程并行加速。

> 四叉树-空间索引加速相交检测资料：
> - [四叉树概念](https://www.cnblogs.com/JimmyZou/p/18385213)
> - [四叉树的基础模版](https://github.com/smileliping/QTree4Cpp)

## 2. 目录结构

```text
EdaChallenge/
├── answer/                         # golden 输出
│   ├── Public/
│   │   ├── small/
│   │   └── large/
│   └── Hidden/
├── checker/                        # Ubuntu x64 答案校验程序
├── doc/                            # 赛题文档和参考资料
├── instance/                       # 测试实例
│   ├── case/
│   │   ├── Public/                 # 公测版图数据（大文件默认 gitignored）
│   │   └── Hidden/                 # 隐藏版图数据（大文件默认 gitignored）
│   └── Rule/
│       ├── Public/                 # 公测规则文件
│       └── Hidden/                 # 隐藏规则文件
├── results/                        # 性能分析 CSV
├── solution/                       # trace 输出结果
│   ├── Public/
│   └── Hidden/
├── test/                           # 测试、分析、可视化脚本
└── trace/                          # C++ 源码
    ├── ManhattanBooleanSetOperation/
    └── *.hpp / *.cpp
```

## 3. 代码结构

| 模块 | 职责 |
|:--|:--|
| `trace/public.h` | 公共类型定义：`Polygon`、`Rect`、`UnionFindSet`、`Timer`、日志宏等。 |
| `trace/Input.hpp` | 解析规则文件和版图文件，按规则过滤需要处理的目标图层，支持 mmap 和并行解析。 |
| `trace/QuadTree.hpp` | 四叉树空间索引，使用内存池分配节点。 |
| `trace/SpaceIndex.hpp` | 四叉树索引上层封装，支持按 Via 关系合并索引或分层索引。 |
| `trace/ManhattanIntersectDetector.hpp` | 曼哈顿多边形相交检测，处理边相交和包含关系。 |
| `trace/Intersect.hpp` | 相交检测编排，支持完整建图和 Lazy BFS 按需邻居查询。 |
| `trace/Graph.hpp` | CSR 格式无向图，支持批量建边和 BFS 连通分量。 |
| `trace/PolygonCutting.hpp` | Gate 规则处理：PO 合并、AA 切割、切割边维护。 |
| `trace/Trace.hpp` | 顶层链路追踪流程，包含 Complete/Lazy 与单线程/多线程策略。 |
| `trace/Output.hpp` | 按 Layer 分组输出连通分量多边形。 |
| `trace/ManhattanBooleanSetOperation/` | 自研曼哈顿布尔运算静态库，替代早期 CGAL 路径。 |
| `trace/NaiveThreadPool.h` | 简易线程池，目前主要作为辅助组件保留。 |
| `trace/robin_hood.h` | 高性能 hash map/set。 |

## 4. 编译

要求：C++17、OpenMP、CMake。当前几何后端使用自研 MBSO，不依赖 `trace/third_party` 中的 CGAL、Boost、GMP 或 MPFR。

```shell
# Windows MSVC
cmake --preset msvc-release
cmake --build --preset msvc-release

# Windows MinGW-w64
cmake --preset mingw-release
cmake --build --preset mingw-release

# Linux GCC
cmake --preset gcc-release
cmake --build --preset gcc-release
```

可执行文件输出到 `bin/trace[.exe]`。常用 preset 包括：

| preset | 平台 | 类型 |
|:--|:--|:--|
| `msvc-release` / `msvc-debug` | Windows MSVC | Release / Debug |
| `mingw-release` / `mingw-debug` | Windows MinGW | Release / Debug |
| `gcc-release` / `gcc-debug` | Linux GCC | Release / Debug |

## 5. 运行

### 5.1 直接运行 trace

```shell
cd bin
./trace -layout <layout_file> -rule <rule_file> [-thread n] -output <output_file>
```

示例：

```shell
./trace \
  -layout ../instance/case/Public/case1_small_layout.txt \
  -rule ../instance/Rule/Public/public_small_rule1.txt \
  -thread 1 \
  -output ../solution/Public/case1_small_layout_q1.txt
```

| 参数 | 说明 |
|:--|:--|
| `-layout` | 必选，版图文件路径。 |
| `-rule` | 必选，规则文件路径。 |
| `-thread` | 可选，并行线程数，默认 1。 |
| `-output` | 必选，输出结果文件路径。 |

### 5.2 单个用例自动化

```shell
python3 test/run_trace.py \
  -layout instance/case/Public/case1_small_layout.txt \
  -rule instance/Rule/Public/public_small_rule1.txt \
  -output solution/Public/case1_small_layout_q1.txt \
  -ans answer/Public/small/case1_small_q1.txt \
  -thread 1 \
  --skip-build
```

常用参数：

| 参数 | 说明 |
|:--|:--|
| `--build-preset <preset>` | 使用 CMake preset 构建后再运行。 |
| `--skip-build` | 跳过构建，直接使用已有 `bin/trace[.exe]`。 |
| `--skip-check` | 跳过 checker 校验。 |

### 5.3 批量测试

```shell
# 默认每个规则重复 3 次，单线程，跳过构建
bash test/test.sh

# 每个规则重复 5 次，8 线程
bash test/test.sh -n 5 -t 8

# 首次使用 gcc-release 构建，后续用例自动跳过重复构建
bash test/test.sh --build-preset gcc-release -n 3 -t 1
```

`test/test.sh` 会按 Public / Hidden 测试集映射运行所有已存在的 rule。每轮日志写入 `logs/<case>_t<thread>_rXX.txt`，每个 rule 完成后生成 `results/<case>_t<thread>.csv`，最后一行为 AVG 平均值。

## 6. 性能分析工具

| 脚本 | 用途 | 示例 |
|:--|:--|:--|
| `test/analysis.py` | 从日志中提取性能指标，生成 CSV。 | `python3 test/analysis.py logs/case1_small_q1_t1_r*.txt -o results/case1_small_q1_t1.csv` |
| `test/visual.py` | 可视化输出多边形结果。 | `python3 test/visual.py solution/Public/res.txt -s output/visual.png` |

`analysis.py` 输出字段包括 `Total_Time`、`Input_Time`、`Space_Index_Time`、`Lazy_BFS_Time`、`Output_Time`、`Merge_Cutting_Time`、`Trace_Wall_Time`、`Pipeline_Time`。CSV 使用 UTF-8 BOM 编码，便于 Windows Excel 直接打开中文列名。

## 7. 代码改动记录

以下按改动文件组织，记录 eda 分支在 C/C++ 代码和 CMake 编译系统上的主要改动及其目的与效果。

### `trace.cpp`

- **改动**：命令行参数中的线程数解析从 `atoi` 替换为 `strtol`。
- **目的**：`atoi` 在输入非法字符串时静默返回 0 或未定义值，`strtol` 通过 `endptr` 和 `errno` 提供完整的错误检测能力。
- **效果**：提升参数解析鲁棒性，可区分空字符串、非数字字符和数值溢出等异常情况。对核心运行性能无影响。

### `Input.hpp`

- **改动**：提取 `readLayout` 和 `readLayoutParallel` 的公共解析逻辑至 `prepareParseTasks` 私有方法；恢复 `preparePolygonVertices` 启发式预估替代硬编码 `reserve(12)`；将关键路径上的 `assert()` 替换为运行时 `if` 判断配合 `std::cerr` 错误输出。
- **目的**：消除约 60 行重复代码；为每个多边形的顶点向量提供更准确的内存预分配；确保 Release 编译模式（`NDEBUG` 定义）下边界检查不会失效。
- **效果**：降低代码维护负担；减少顶点向量在解析过程中的 reallocation 次数；避免 Release 模式下因断言失效导致的潜在未定义行为。

### `HighPerfMemoryPool.h` / `QuadTree.hpp`

- **改动**：四叉树节点内存池 `quadTreeNodePool` 从 `thread_local` 改为普通 `static` 全局变量，在 `HighPerfMemoryPool` 类中引入 `std::mutex` 保护自由链表的并发访问；MBSO 内部小池（`MVertex` / `MEdge`）保持无锁快路径以保障布尔运算的分配性能；`total_allocated_` 和 `total_freed_` 计数器纳入 mutex 保护区以消除多线程数据竞争。
- **目的**：原 `thread_local` 池在并行构建与清理两个阶段存在跨线程释放风险——线程 A 分配的节点可能被线程 B 在清理阶段释放，而 `thread_local` 池在不同线程间相互隔离，导致节点被归还到一个它从未分配过的池中。
- **效果**：彻底消除跨线程释放导致的未定义行为；多线程构建四叉树的安全性得到保障。MBSO 小池的无锁设计确保了布尔运算的高频分配/释放路径不受全局锁的干扰。

### `MemoryPool` 批量化实验（已回退）

- **改动**：大池分配路径增加线程本地批量缓存（`local_cache`），每攒满 256 个节点才获取一次 mutex 批量 refill；释放路径对称增加 `local_free_cache` 批量归还。
- **目的**：降低并行建索引时多个线程对全局 mutex 的竞争频率。
- **效果**：经实测，单线程下 TLS 查询和条件分支开销超过了无竞争 mutex 的原子操作成本（large\_q2 Space Index +14%）；多线程大数据集下也未观察到加速。最终禁用批量化（`LOCAL_CACHE_POOL_THRESHOLD` → `SIZE_MAX`），恢复简单 `std::lock_guard` 路径。该实验证实了在低竞争场景下简洁的 mutex 方案优于复杂的 TLS 批量化设计。

### `Graph.hpp`

- **改动**：BFS visited 标记数组从 `std::vector<bool>` 替换为 `std::vector<uint8_t>`；`AddEdges` 方法对越界边从静默跳过改为输出一次性 `std::cerr` 警告。
- **目的**：C++ 标准库对 `vector<bool>` 的特化将每个布尔值压缩为 1 位存储，每次访问需要位掩码操作并返回代理对象而非真正的引用，阻碍了编译器的向量化优化。
- **效果**：visited 访问变为直接字节读写，消除位操作间接开销；越界边不再被静默丢弃，便于排查输入数据异常。

### `Intersect.hpp`

- **改动**：`GetNeighborsLazy` 和 `GetNeighborsLazyParallel` 去除中间 `leaf_buffer` 向量，直接在四叉树的深度优先递归遍历中处理候选多边形并执行相交检测；移除过小的 `reserve(256)` 让 `neighbor_candidate_cache` 的 `robin_hood::unordered_set` 复用已增长的容量。
- **目的**：原实现先收集与源多边形相交的叶节点到中间向量，再二次遍历处理候选——引入了不必要的堆分配和间接访问。
- **效果**：减少 Lazy BFS 邻居查询的中间内存分配；消除候选集因过小 reserve 导致的重复 rehash。

### `Lazy BFS` 候选去重实验（已回退）

- **改动**：在 `Intersect.hpp` 的 `CollectAndProcessLeaves` 系列方法中对候选多边形执行 `robin_hood::unordered_set::insert` 去重，同一多边形跨多个叶节点时仅检测一次相交。
- **目的**：避免重复多边形对之间的相交检测。
- **效果**：经实测，hash insert 的开销远超去重收益，large\_q2 单线程 Lazy BFS 耗时增加 55%。最终移除去重逻辑，恢复无去重的直接检测。该实验表明在叶节点尺寸较小（max 10 多边形）的约束下，重复候选的发生频率不足以支撑去重的计算成本。

### `Trace.hpp`

- **改动**：`RunLazyConnectedComponentParallel` 中串行回退路径（当前沿层级节点数 < 100）改用非原子版本的 `GetNeighborsLazy`。
- **目的**：串行路径中不存在多线程并发访问 visited 数组的情况，使用 `__atomic_load_n` 读取 visited 状态完全多余。
- **效果**：消除了并行 BFS 前期小批次阶段的原子操作开销，允许编译器对串行路径进行更激进的指令重排和寄存器缓存优化。

### `QuadTree.hpp` / `SpaceIndex.hpp` / `PolygonCutting.hpp`

- **改动**：`CreatIndex` 系列方法的参数类型从 `const std::vector<Polygon*>&` 改为按值传递 `std::vector<Polygon*>`，调用方通过 `std::move` 将多边形指针向量的所有权直接转移给四叉树。
- **目的**：在输入向量包含百万级指针时避免完整拷贝带来的数 MB 到数十 MB 的内存复制开销。
- **效果**：降低 Space Index 构建阶段的内存带宽占用。调用方需根据是否需要保留原向量来决定是否使用 `std::move`。

### `ManhattanIntersectDetector.hpp`

- **改动**：修复边包含判断中的链式比较缺陷（`a < b < c` 改为显式区间检查 `b > a && b < c`）。
- **目的**：C++ 中 `a < b < c` 被解析为 `(a < b) < c`，即布尔值与整数的比较，而不是区间判断。
- **效果**：消除潜在的几何判定错误。

### OpenMP 并行区（全局）

- **改动**：移除全部 `omp_set_num_threads()` 全局调用，改为在每个并行区域显式使用 `num_threads(thread_count)` 子句。
- **目的**：`omp_set_num_threads()` 的全局副作用会渗透到后续所有未显式指定线程数的并行区域，导致不可预期的线程分配行为。
- **效果**：线程数控制精确到每个并行区域，消除了全局状态污染。

## 8. 性能记录

### 服务器参数

| 项目 | 参数 |
|:--|:--|
| CPU | Intel(R) Xeon(R) Platinum 8255C CPU @ 2.50GHz |
| 核心数 | 96 |
| 内存 | 375.80 GB |
| 操作系统 | Ubuntu 22.04.1 LTS |
| 内核 | 5.4.0-126-generic |
| 架构 | x86_64 |

### 测试-单线程-优化前基线 (MBSO+Lazy BFS, 初始版本)
> 注意：Hidden Case 由于原作者未测试，因此这里不给出数据，**eda** 分支已经实现 Hidden Case的布线，`test/test.sh` 中已经包含批量测试

|            case          |  rule  | Total Time |   Input   | Space Index |  Lazy BFS  |   Output   | Merge/Cutting |
|:------------------------:|:------:|:----------:|:---------:|:-----------:|:----------:|:----------:|:-------------:|
|    case1_small_layout    |   q1   |  0.116s | 0.066s |  0.031s  | 0.001s  |  0.006s |       -       |
|    case1_small_layout    |   q2   |  2.696s | 0.245s |  0.161s  | 1.869s  |  0.340s |       -       |
|    case1_small_layout    |   q3   |  2.440s | 0.194s |  0.139s  | 1.300s  |  0.227s |   0.426s   |
| case1_large_0909b_layout |   q1   | 16.627s | 4.558s |  1.963s  | 7.543s  |  2.004s |       -       |
| case1_large_0909b_layout |   q2   | 76.602s | 15.903s | 13.452s  | 38.032s  |  5.327s |       -       |
| case1_large_0909b_layout |   q3   |172.898s | 16.691s | 15.754s  | 0.036s  | 12.911s |  29.414s   |
|    hidden_case_layout    |   q1   |      -     |   -       |      -      |    -       |   -        |       -       |
|    hidden_case_layout    |   q2   |      -     |   -       |      -      |    -       |   -        |       -       |
|    hidden_case_layout    |   q3   |      -     |   -       |      -      |    -       |   -        |       -       |

### 测试-单线程-优化后 (MBSO+Lazy BFS, eda 分支)

|            case          |  rule  | Total Time |   Input   | Space Index |  Lazy BFS  |   Output   | Merge/Cutting |
|:------------------------:|:------:|:----------:|:---------:|:-----------:|:----------:|:----------:|:-------------:|
|    case1_small_layout    |   q1   |  0.107s | 0.058s |  0.037s  | 0.001s  |  0.005s |       -       |
|    case1_small_layout    |   q2   |  2.019s | 0.218s |  0.163s  | 1.326s  |  0.286s |       -       |
|    case1_small_layout    |   q3   |  2.010s | 0.171s |  0.133s  | 1.031s  |  0.217s |   0.355s   |
| case1_large_0909b_layout |   q1   | 16.260s | 4.062s |  1.933s  | 7.930s  |  2.078s |       -       |
| case1_large_0909b_layout |   q2   | 63.255s | 14.517s | 13.346s  | 27.640s  |  5.633s |       -       |
| case1_large_0909b_layout |   q3   |140.013s | 14.858s | 15.425s  | 0.032s  | 14.639s |  24.749s   |
|    hidden_case_layout    |   q1   |      -     |   -       |      -      |    -       |   -        |       -       |
|    hidden_case_layout    |   q2   |      -     |   -       |      -      |    -       |   -        |       -       |
|    hidden_case_layout    |   q3   |      -     |   -       |      -      |    -       |   -        |       -       |

### 测试-多线程(8)-优化前基线 (MBSO+Lazy BFS, 初始版本)

|            case          |  rule  | Total Time |   Input   | Space Index |  Lazy BFS  |   Output   | Merge/Cutting |
|:------------------------:|:------:|:----------:|:---------:|:-----------:|:----------:|:----------:|:-------------:|
|    case1_small_layout    |   q1   |  0.120s | 0.067s |  0.033s  | 0.001s  |  0.003s |       -       |
|    case1_small_layout    |   q2   |  0.824s | 0.102s |  0.176s  | 0.237s  |  0.216s |       -       |
|    case1_small_layout    |   q3   |  0.957s | 0.088s |  0.159s  | 0.184s  |  0.176s |   0.206s   |
| case1_large_0909b_layout |   q1   |  6.909s | 2.520s |  0.638s  | 1.437s  |  1.217s |       -       |
| case1_large_0909b_layout |   q2   | 27.365s | 6.728s |  3.434s  | 6.624s  |  3.576s |       -       |
| case1_large_0909b_layout |   q3   | 55.416s | 7.256s |  4.117s  | 0.052s  |  8.373s |  11.418s   |
|    hidden_case_layout    |   q1   |      -     |   -       |      -      |    -       |   -        |       -       |
|    hidden_case_layout    |   q2   |      -     |   -       |      -      |    -       |   -        |       -       |
|    hidden_case_layout    |   q3   |      -     |   -       |      -      |    -       |   -        |       -       |

### 测试-多线程(8)-优化后 (MBSO+Lazy BFS, eda 分支)

|            case          |  rule  | Total Time |   Input   | Space Index |  Lazy BFS  |   Output   | Merge/Cutting |
|:------------------------:|:------:|:----------:|:---------:|:-----------:|:----------:|:----------:|:-------------:|
|    case1_small_layout    |   q1   |  0.145s | 0.066s |  0.058s  | 0.001s  |  0.003s |       -       |
|    case1_small_layout    |   q2   |  0.687s | 0.103s |  0.083s  | 0.242s  |  0.220s |       -       |
|    case1_small_layout    |   q3   |  0.722s | 0.082s |  0.031s  | 0.185s  |  0.174s |   0.166s   |
| case1_large_0909b_layout |   q1   |  5.824s | 2.476s |  0.583s  | 1.306s  |  1.100s |       -       |
| case1_large_0909b_layout |   q2   | 20.485s | 6.254s |  3.024s  | 4.714s  |  4.032s |       -       |
| case1_large_0909b_layout |   q3   | 44.006s | 6.416s |  3.549s  | 0.039s  |  8.855s |  10.315s   |
|    hidden_case_layout    |   q1   |      -     |   -       |      -      |    -       |   -        |       -       |
|    hidden_case_layout    |   q2   |      -     |   -       |      -      |    -       |   -        |       -       |
|    hidden_case_layout    |   q3   |      -     |   -       |      -      |    -       |   -        |       -       |