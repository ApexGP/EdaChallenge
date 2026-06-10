# 2025 中国研究生创“芯”大赛·EDA 精英挑战赛
## 赛题三：版图多层链路追踪算法
赛题描述： [EDA赛题三](./doc/eda赛题三.pdf)

## 1.问题分析
本质是一个P问题，预处理建图之后，只需根据起点结点求连通分量。
难点：
+ 多边形数量在千万级到亿级之间，规模很大，如何设计数据存储结构？
+ 如何快速进行多边形之间的相交检测？
+ 如何利用多线程加速？

### 1.1 四叉树-空间索引加速相交检测
四叉树资料：
+ [四叉树概念](https://www.cnblogs.com/JimmyZou/p/18385213)
+ [四叉树的基础模版](https://github.com/smileliping/QTree4Cpp)

## 2.目录结构
+ doc/ 文档
+ instance/ 算例
+ solution/ 结果
+ trace/ 源码
  + third_party/ 第三方库（gitignored）：Boost（头文件）、CGAL（头文件，当前未使用）、GMP/MPFR 预编译库
  + ManhattanBooleanSetOperation/ 自研曼哈顿布尔运算静态库（`libMBSO.a`），替代了早期的 CGAL 依赖
+ test/ 测试
+ checker/ 检验答案程序 查阅 [检验程序说明](./checker/README.md)


## 3.代码结构
+ `public.h` 公用数据定义（Polygon、Rect、UnionFindSet、Timer 等）
+ `NaiveThreadPool.h` 简单线程池
+ `robin_hood.h` 高性能 hash map/set，替代 `std::unordered_map`
+ `Input.hpp` 输入类：mmap 加速读取，支持按 chunk 并行解析
+ `Intersect.hpp` 多边形相交检测编排（完整建图 / 惰性按需两种模式）
+ `ManhattanIntersectDetector.hpp` 曼哈顿多边形专用相交检测（基于两边比较和点包含）
+ `QuadTree.hpp` 四叉树空间索引（线程安全内存池分配）
+ `SpaceIndex.hpp` 四叉树空间索引上层封装（支持 Via 层合并）
+ `Graph.hpp` CSR 格式无向图：批量建边 + BFS 连通分量
+ `PolygonCutting.hpp` Gate 规则处理：PO 层合并、AA 层切割
+ `Trace.hpp` 链路追踪上层封装（4 种策略：Complete/Lazy × 单线程/多线程）
+ `Output.hpp` 结果输出
+ `ManhattanBooleanSetOperation/` 自研静态库：替代 CGAL 布尔集合运算

## 4.运行方式
### 4.1.环境
C++17 及以上。支持 **MSVC / MinGW-w64 / GCC** 三种编译器，通过 CMake Presets 一键配置。

**必需依赖**：OpenMP（CMake 自动检测，未找到时报错提示安装）。

**可选依赖**：GMP/MPFR（仅 CGAL 需要，当前 MBSO 路径不依赖）。

### 4.2.编译

```shell
# Windows MSVC
cmake --preset msvc-release && cmake --build --preset msvc-release

# Windows MinGW-w64
cmake --preset mingw-release && cmake --build --preset mingw-release

# Linux GCC
cmake --preset gcc-release && cmake --build --preset gcc-release
```

所有 preset 均提供 `-release` / `-debug` 变体（共 6 组）。可执行文件输出到 `bin/trace[.exe]`。

优化参数按平台自动选择：
- **MSVC**: `/O2 /arch:AVX2 /fp:fast /MT`
- **MinGW/GCC**: `-O3 -flto -mavx2 -mfma -mbmi2 -mpopcnt -march=x86-64-v3 -static`

### 4.3.运行

```shell
cd ../bin
./trace -layout <layout_file> -rule <rule_file> [-thread n] -output <output_file>
# 例: ./trace -layout ../instance/case/case1_small_layout.txt \
#             -rule ../instance/Rule/public_small_rule1.txt \
#             -output ../solution/case1_small_layout_q1.txt
```

| 参数 | 说明 |
|:-----|:-----|
| `-layout` | 必选，版图文件路径 |
| `-rule` | 必选，规则文件路径 |
| `-thread` | 可选，并行线程数（默认 1，范围 1-256） |
| `-output` | 必选，输出结果文件路径 |

### 4.4.Python 脚本自动化

```shell
cd test
python3 run_trace.py -layout <layout_file> -rule <rule_file> [-thread n] -output <res_file> -ans <ans_file>
```
脚本自动完成 cmake/make 编译、运行 trace、调用 checker 验证结果。

## 5.可视化脚本
test/visual.py
调用方式：
```shell
# 默认
python3 visual.py ../solution/res.txt
# 在每个多边形内部显示层名和编号 -l 或 --label 参数
python3 visual.py ../solution/res.txt -l
python3 visual.py ../solution/res.txt --label
# 设置图片保存路径 -s 或 --save 参数
python3 visual.py ../solution/res.txt -s ./output.png
```

## 6.打包提交
test/submit.py  自动编译提取打包压缩
调用方式：
```shell
# 基本用法 "my_submission" 指定压缩文件名，未指定线程数则默认写入1
python3 submit.py my_submission
# 指定线程数，写入thread.txt文件
python3 submit.py my_submission 8
```

## 7.Simple Test
环境:3.5Ghz CPU, 32G RAM, Intel(R) Core(TM) i3-14100
### 测试-单线程-Windows VS2022-Using CGAL do_intersect
|            case          |  rule  | n^2暴力 |  空间索引 |  矩形框初筛 |  索引+初筛  | 索引+初筛+提前跳出 |
|:------------------------:|:------:|:-------:|:---------:|:-----------:|:----------:|:------------------:|
|    case1_small_layout    |   q1   |    -    |   63.6s   |     33.6s   |    31.0s   |        28.1s       |
|    case1_small_layout    |   q2   |    -    |  485.8s   |    736.1s   |   238.6s   |       219.7s       |
| case1_large_0909b_layout |   q1   |    -    |     -     |      -      |  1675.1s   |      1611.2s       |
| case1_large_0909b_layout |   q2   |    -    |     -     |      -      |      -     |     12692.9s       |

Why? 实测Ubuntu系统运行比Windows快一些，猜测内存分配速度是主要性能差距来源

### 测试-单线程-Ubuntu-22.04 Using High Performance Implementation Sep 24, 2025 PS : using CGAL BooleanSetOperation
|            case          |  rule  | High Performance Optimization |
|:------------------------:|:------:|:-----------------------------:|
|    case1_small_layout    |   q1   |              0.36s            |
|    case1_small_layout    |   q2   |              2.17s            |
|    case1_small_layout    |   q3   |              5.10s            |
| case1_large_0909b_layout |   q1   |              22.4s            |
| case1_large_0909b_layout |   q2   |             124.2s            |
| case1_large_0909b_layout |   q3   |             356.3s            |

### 测试-单线程-Ubuntu-22.04 Using High Performance Implementation 10月2日最新版 PS : remove all CGAL and using self implement BooleanSetOperation
|            case          |  rule  | High Performance Optimization |
|:------------------------:|:------:|:-----------------------------:|
|    case1_small_layout    |   q1   |              0.28s            |
|    case1_small_layout    |   q2   |              1.83s            |
|    case1_small_layout    |   q3   |              2.15s            |
| case1_large_0909b_layout |   q1   |              20.2s            |
| case1_large_0909b_layout |   q2   |             102.5s            |
| case1_large_0909b_layout |   q3   |             148.5s            |

### 测试-单线程-Ubuntu-22.04 Using High Performance Implementation 10月13日最新版 PS : optimal BooleanSetOperation and some memory recycle optimal
|            case          |  rule  | High Performance Optimization |
|:------------------------:|:------:|:-----------------------------:|
|    case1_small_layout    |   q1   |              0.27s            |
|    case1_small_layout    |   q2   |              1.36s            |
|    case1_small_layout    |   q3   |              1.49s            |
| case1_large_0909b_layout |   q1   |              16.5s            |
| case1_large_0909b_layout |   q2   |              80.5s            |
| case1_large_0909b_layout |   q3   |             115.8s            |

### 测试-单线程-Ubuntu-22.04 Using High Performance Implementation 10月15日最新版 PS : using Lazy BFS
|            case          |  rule  | High Performance Optimization |
|:------------------------:|:------:|:-----------------------------:|
|    case1_small_layout    |   q1   |              0.14s            |
|    case1_small_layout    |   q2   |              1.39s            |
|    case1_small_layout    |   q3   |              1.89s            |
| case1_large_0909b_layout |   q1   |              15.6s            |
| case1_large_0909b_layout |   q2   |              39.9s            |
| case1_large_0909b_layout |   q3   |              88.0s            |

### 测试-单线程-Ubuntu-22.04 Using High Performance Implementation 10月31日最新版 PS : 各种细节调优 + lazy input:只输入需要的层
|            case          |  rule  | High Performance Optimization |
|:------------------------:|:------:|:-----------------------------:|
|    case1_small_layout    |   q1   |              0.08s            |
|    case1_small_layout    |   q2   |              1.13s            |
|    case1_small_layout    |   q3   |              1.59s            |
| case1_large_0909b_layout |   q1   |              11.8s            |
| case1_large_0909b_layout |   q2   |              31.7s            |
| case1_large_0909b_layout |   q3   |              77.4s            |

### 测试-多线程(8)-Ubuntu-22.04 Using High Performance Implementation 10月31日最新版 PS : openMP并行，整体流程仍采用串行，结合并行处理加速
|            case          |  rule  | High Performance Optimization |
|:------------------------:|:------:|:-----------------------------:|
|    case1_small_layout    |   q1   |              0.07s            |
|    case1_small_layout    |   q2   |              0.40s            |
|    case1_small_layout    |   q3   |              0.61s            |
| case1_large_0909b_layout |   q1   |              6.50s            |
| case1_large_0909b_layout |   q2   |              15.6s            |
| case1_large_0909b_layout |   q3   |              30.5s            |