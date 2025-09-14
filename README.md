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
+ test/ 测试

## 3.代码结构
+ Input.hpp 输入类
+ Intersect.hpp 多边形相交检测类
+ Graph.hpp 拓扑图类，根据边集建图，求连通分量
+ Output.hpp 输出类

## 4.运行方式
### Windows with VS2022


### Linux


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