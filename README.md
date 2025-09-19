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
+ + thirty/ 第三方库, 更多信息请查阅 [第三方库说明](./trace/third_party/README.md)
+ test/ 测试
+ trace_answer_checker/ 检验答案程序


## 3.代码结构
+ Input.hpp 输入类
+ Intersect.hpp 多边形相交检测类
+ QuadTree.hpp 四叉树实现类
+ SpaceIndex.hpp 使用四叉树建立空间索引类
+ Graph.hpp 拓扑图类，根据边集建图，求连通分量
+ Output.hpp 输出类

## 4.运行方式
### 环境
​由于我们使用CGAL6.0+​​, 所以要求​C++17及以上, 即编译器要求: GCC 7+, Clang 5+, MSVC 2017+

### Windows with VS2022
配置包含目录和库目录，以及链接器-附加依赖项，VC++17

### Linux
```shell
cd trace
# 编译
mkdir build
cd build
cmake ..
make
# 运行
trace -layout ./layout.txt -rule ./rule.txt [-thread n] -output ./res.txt 
```
+ trace：链路追踪（Net Trace）可执行程序
+ -layout：必选参数，指定版图文件的路径，该文件由出题方提供。
+ -rule:  必选参数，指定规则文件的路径，该文件由出题方提供。
+ -thread：可选参数，指定n个线程的并行计算。
+ -output:  必须参数，指定输出的结果文件的路径，由程序运行产生。

## 5.可视化脚本
test/visual.py  
调用方式：  
```shell
# 默认
python visual.py ../solution/res.txt
# 在每个多边形内部显示层名和编号 -l 或 --label 参数
python visual.py ../solution/res.txt -l
python visual.py ../solution/res.txt --label
```

## 6.Simple Test
### 相交检测测试-单线程
|            case          |  rule  | n^2暴力 |  空间索引 |  矩形框初筛 |  索引+初筛  | 索引+初筛+提前跳出 |
|:------------------------:|:------:|:-------:|:---------:|:-----------:|:----------:|:------------------:|
|    case1_small_layout    |   q1   |    -    |   63.6s   |     33.6s   |    31.0s   |        28.1s       |
|    case1_small_layout    |   q2   |    -    |  485.8s   |    736.1s   |   238.6s   |       219.7s       |
| case1_large_0909b_layout |   q1   |    -    |     -     |      -      |  1889.2s ?  |     1611.2s       |
| case1_large_0909b_layout |   q2   |    -    |     -     |      -      |      -     |     12692.9s       |