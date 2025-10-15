#pragma once
#include "public.h"

// 四叉树节点定义
class QuadTreeNode
{
public:
    Rect _rect;                     // 节点的矩形空间范围（包含子节点的范围）
    bool _divided = false;          // 是否已划分
    std::vector<Polygon*> _datas;   // 节点数据，存储多边形指针
    int _depth;                     // 节点深度

    // 四个子节点
    QuadTreeNode* _lt;    // 左上
    QuadTreeNode* _rt;    // 右上  
    QuadTreeNode* _lb;    // 左下
    QuadTreeNode* _rb;    // 右下

    // 构造函数：仅包含矩形范围和深度
    explicit QuadTreeNode(Rect rect, int depth) : _rect(rect), _lt(nullptr), _rt(nullptr)
        , _lb(nullptr), _rb(nullptr), _depth(depth), _divided(false) {
    }

    // 构造函数：包含矩形范围、深度和初始数据
    explicit QuadTreeNode(Rect rect, int depth, std::vector<Polygon*>& datas) : QuadTreeNode(rect, depth)
    {
        _datas = datas;
    }

    // 禁止拷贝构造和赋值操作
    QuadTreeNode(const QuadTreeNode&) = delete;
    QuadTreeNode& operator=(const QuadTreeNode&) = delete;

    // 析构函数
    ~QuadTreeNode() {
        clear();
    }

    // 清空节点数据并释放子节点内存
    void clear() {
        _datas.clear(); // 清空数据

        // 手动删除所有子节点
        if (_lt) {
            delete _lt; _lt = nullptr;
        }
        if (_rt) {
            delete _rt; _rt = nullptr;
        }
        if (_lb) {
            delete _lb; _lb = nullptr;
        }
        if (_rb) {
            delete _rb; _rb = nullptr;
        }
    }

    // 判断指定点位置是否在节点矩形内部或边界上
    bool Contains(const Point& p) const
    {
        return _rect.Contains(p.first, p.second);
    }
};

// 四叉树类
class QuadTree {
public:
    std::string _name;      // 四叉树名称
    QuadTreeNode* _root;    // 根节点
    int _maxDepth;          // 最大深度限制
    int _maxDataNum;        // 叶节点最大数据量限制
    int _maxCurrDepth;      // 当前实际达到的最大深度
    int _maxCurrDataNum;    // 当前叶节点实际最大数据量

public:
    /*
    * brief 四叉树构造函数
    * rect: 根节点的矩形空间
    * maxDepth: 四叉树最大深度（根节点深度=1）
    * maxDataNum: 四叉树叶节点数据量限制
    * name: 四叉树名称
    */
    QuadTree(const Rect& rect, int maxDepth, int maxDataNum, const std::string& name = "QUADTREE")
        : _root(new QuadTreeNode(rect, 1)), _maxDepth(maxDepth), _maxDataNum(maxDataNum), _name(name) {
        _maxCurrDepth = 1;
        _maxCurrDataNum = 0;
    }

    // 析构函数
    ~QuadTree() {
        delete _root;
    }

    // 禁止拷贝构造和赋值操作
    QuadTree(const QuadTree&) = delete;
    QuadTree& operator=(const QuadTree&) = delete;

    /*
    * brief 根据多边形的矩形包围盒构建四叉树空间索引
    * poly_ptr: 多边形指针向量
    * return: 构建是否成功
    */
    bool CreatIndex(std::vector<Polygon*>& poly_ptr) {
        clear();

        // 初始化根节点数据
        _root->_datas = poly_ptr;

        // 递归划分节点
        return SplitNode(_root);
    }

    // 插入功能（待实现）
    bool Insert() { return false; }

    // 获取四叉树所有叶节点的数据
    void GetAllLeafData(std::vector<std::vector<Polygon*>>& outData) {
        GetAllLeafData(_root, outData);
    }

    // 获取四叉树所有叶节点的数据和对应的矩形范围
    void GetAllLeafData(std::vector<std::vector<Polygon*>>& outData, std::vector<Rect>& outRect) {
        GetAllLeafData(_root, outData, outRect);
    }

    // 根据矩形范围查询相交的多边形
    void Query(const Rect& region, std::vector<Polygon*>& result) const {
        Query(_root, region, result);
    }

    // 收集与指定区域相交的所有非空叶节点
    void CollectIntersectLeaves(const Rect& region, std::vector<QuadTreeNode*>& leaves) const {
        CollectIntersectLeaves(_root, region, leaves);
    }

    // 获取包含指定点的叶节点数据
    const std::vector<Polygon*>* GetLeafDataofPoint(const Point& point) const {
        QuadTreeNode* node = SearchNodeofPoint(_root, point);
        return node ? &node->_datas : nullptr;
    }

    // 分析功能：获取四叉树所有节点的矩形范围
    void GetAllNodeRect(std::vector<Rect>& rects) {
        GetAllNodeRect(_root, rects);
    }

    // 分析功能：获取四叉树叶节点数量
    int GetLeafNum() {
        return GetLeafNum(_root);
    }

private:
    // 清空四叉树
    void clear() { _root->clear(); }

    // 递归划分节点
    bool SplitNode(QuadTreeNode* node)
    {
        if (node == nullptr) return false;
        const size_t data_size = node->_datas.size();

        // 节点深度达到最大深度或者节点数据量小于等于限制值，停止划分
        if (node->_depth >= _maxDepth) {
            _maxCurrDataNum = static_cast<int>((node->_datas.size() > _maxCurrDataNum) ? node->_datas.size() : _maxCurrDataNum);
            return true;
        }
        if (node->_datas.size() <= _maxDataNum) {
            _maxCurrDataNum = static_cast<int>((node->_datas.size() > _maxCurrDataNum) ? node->_datas.size() : _maxCurrDataNum);
            return true;
        }

        // 更新当前实际深度
        _maxCurrDepth = ((node->_depth + 1 > _maxCurrDepth) ? node->_depth + 1 : _maxCurrDepth);

        // 预计算四个子矩形范围
        const Rect& parent_rect = node->_rect;
        const Rect lt_rect = parent_rect.GetLTRect();
        const Rect rt_rect = parent_rect.GetRTRect();
        const Rect lb_rect = parent_rect.GetLBRect();
        const Rect rb_rect = parent_rect.GetRBRect();

        // 创建四个子节点
        node->_divided = true;
        node->_lt = new QuadTreeNode(lt_rect, node->_depth + 1);
        node->_rt = new QuadTreeNode(rt_rect, node->_depth + 1);
        node->_lb = new QuadTreeNode(lb_rect, node->_depth + 1);
        node->_rb = new QuadTreeNode(rb_rect, node->_depth + 1);

        // 为数据分配预留空间（优化性能）
        size_t estimated_size = (data_size / 3); // 保守估计，避免过度分配
        node->_lt->_datas.reserve(estimated_size);
        node->_rt->_datas.reserve(estimated_size);
        node->_lb->_datas.reserve(estimated_size);
        node->_rb->_datas.reserve(estimated_size);

        // 将数据分配到四个子节点中
        const std::vector<Polygon*>& parent_data = node->_datas;
        for (size_t i = 0; i < data_size; i++)
        {
            Polygon* ptr = parent_data[i];
            const Rect& curr_rect = ptr->rect;

            // 根据矩形相交关系，将数据分配到相交的子节点中
            // 注意：与多个子节点矩形相交的数据会被分配到多个子节点中
            if (lt_rect.Intersects(curr_rect)) node->_lt->_datas.push_back(ptr);
            if (rt_rect.Intersects(curr_rect)) node->_rt->_datas.push_back(ptr);
            if (lb_rect.Intersects(curr_rect)) node->_lb->_datas.push_back(ptr);
            if (rb_rect.Intersects(curr_rect)) node->_rb->_datas.push_back(ptr);
        }

        // 释放父节点数据内存（使用swap技巧真正释放内存）
        std::vector<Polygon*>().swap(node->_datas);

        // 递归处理子节点
        bool result = true;
        result &= SplitNode(node->_lt);
        result &= SplitNode(node->_rt);
        result &= SplitNode(node->_lb);
        result &= SplitNode(node->_rb);

        return result;
    }

    // 递归获取所有叶节点数据
    void GetAllLeafData(QuadTreeNode* node, std::vector<std::vector<Polygon*>>& outData) {
        if (!node) return;

        // 叶节点：收集非空数据
        if (!node->_divided) {
            if (node->_datas.size() != 0)
                outData.push_back(node->_datas);
        }
        else {
            // 非叶节点：递归处理子节点
            GetAllLeafData(node->_lt, outData);
            GetAllLeafData(node->_rt, outData);
            GetAllLeafData(node->_lb, outData);
            GetAllLeafData(node->_rb, outData);
        }
    }

    // 递归获取所有叶节点数据和对应的矩形范围
    void GetAllLeafData(QuadTreeNode* node, std::vector<std::vector<Polygon*>>& outData, std::vector<Rect>& outRect) {
        if (!node) return;

        // 叶节点：收集非空数据和矩形范围
        if (!node->_divided) {
            if (node->_datas.size() != 0) {
                outData.push_back(node->_datas);
                outRect.push_back(node->_rect);
            }
        }
        else {
            // 非叶节点：递归处理子节点
            GetAllLeafData(node->_lt, outData, outRect);
            GetAllLeafData(node->_rt, outData, outRect);
            GetAllLeafData(node->_lb, outData, outRect);
            GetAllLeafData(node->_rb, outData, outRect);
        }
    }

    // 递归查询与指定区域相交的多边形
    void Query(QuadTreeNode* node, const Rect& region, std::vector<Polygon*>& result) const {
        if (!node) return;

        // 如果节点矩形与查询区域不相交，直接返回
        if (!node->_rect.Intersects(region)) return;

        // 叶节点：检查每个多边形是否与查询区域相交
        if (!node->_divided) {
            for (auto* poly : node->_datas) {
                if (poly && poly->rect.Intersects(region)) {
                    result.push_back(poly);
                }
            }
            return;
        }

        // 非叶节点：递归查询子节点
        Query(node->_lt, region, result);
        Query(node->_rt, region, result);
        Query(node->_lb, region, result);
        Query(node->_rb, region, result);
    }

    // 递归收集与指定区域相交的所有非空叶节点
    void CollectIntersectLeaves(QuadTreeNode* node, const Rect& region, std::vector<QuadTreeNode*>& leaves) const {
        if (!node) return;

        // 如果节点矩形与查询区域不相交，直接返回
        if (!node->_rect.Intersects(region)) return;

        // 非空叶节点：添加到结果集
        if (!node->_divided && node->_datas.size() != 0) {
            leaves.push_back(node);
            return;
        }

        // 非叶节点：递归处理子节点
        CollectIntersectLeaves(node->_lt, region, leaves);
        CollectIntersectLeaves(node->_rt, region, leaves);
        CollectIntersectLeaves(node->_lb, region, leaves);
        CollectIntersectLeaves(node->_rb, region, leaves);
    }

    // 递归搜索包含指定点的叶节点
    QuadTreeNode* SearchNodeofPoint(QuadTreeNode* node, const Point& point) const {
        if (!node || !node->_rect.Contains(point.first, point.second)) return nullptr;

        // 计算节点中心点坐标
        int xmid = (node->_rect._xmin + node->_rect._xmax) / 2;
        int ymid = (node->_rect._ymin + node->_rect._ymax) / 2;

        // 判断点位于哪个象限
        bool left = point.first <= xmid;
        bool top = point.second >= ymid;

        // 选择对应的子节点
        QuadTreeNode* child = nullptr;
        if (top) {
            child = left ? node->_lt : node->_rt;
        }
        else {
            child = left ? node->_lb : node->_rb;
        }

        // 递归搜索子节点
        if (child) {
            QuadTreeNode* res = SearchNodeofPoint(child, point);
            if (res) return res;
        }

        // 如果子节点不存在或不包含该点，返回当前节点
        return node;
    }

    // 递归获取所有节点的矩形范围
    void GetAllNodeRect(QuadTreeNode* node, std::vector<Rect>& rects) {
        if (!node) return;
        rects.push_back(node->_rect);
        GetAllNodeRect(node->_lt, rects);
        GetAllNodeRect(node->_rt, rects);
        GetAllNodeRect(node->_lb, rects);
        GetAllNodeRect(node->_rb, rects);
    }

    // 递归计算叶节点数量
    int GetLeafNum(QuadTreeNode* node) {
        if (!node) return 0;

        // 叶节点：计数1
        if (!node->_divided) {
            return 1;
        }

        // 非叶节点：递归计算子节点叶节点数量之和
        int leaf_num = 0;
        leaf_num += GetLeafNum(node->_lt);
        leaf_num += GetLeafNum(node->_rt);
        leaf_num += GetLeafNum(node->_lb);
        leaf_num += GetLeafNum(node->_rb);
        return leaf_num;
    }
};