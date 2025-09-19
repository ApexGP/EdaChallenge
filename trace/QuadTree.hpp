#pragma once
#include "public.h"

// 四叉树节点定义
class QuadTreeNode
{
public: 
    Rect _rect;                     // 节点的矩形空间范围，所有子节点的范围的并集
    bool _divided = false;          // 是否已划分
    std::vector<Polygon*> _datas;   // 节点数据, 存指针
    int _depth;                     // 节点的深度

    // 四个子节点
    QuadTreeNode* _lt;    // 左上
    QuadTreeNode* _rt;    // 右上
    QuadTreeNode* _lb;    // 左下
    QuadTreeNode* _rb;    // 右下

    explicit QuadTreeNode(Rect rect, int depth) : _rect(rect), _lt(nullptr), _rt(nullptr)
        ,_lb(nullptr), _rb(nullptr), _depth(depth), _divided(false) {}

    explicit QuadTreeNode(Rect rect, int depth, std::vector<Polygon*>& datas) : QuadTreeNode(rect, depth)
    {
        _datas = datas;
    }

    // 禁止复制
    QuadTreeNode(const QuadTreeNode&) = delete;
    QuadTreeNode& operator=(const QuadTreeNode&) = delete;

    ~QuadTreeNode() {
        clear();
    }

    // 清空操作
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

    // 判断指定的点位是否在节点矩形内部或边界上
    bool Contains(const Point& p) const
    {
        return _rect.Contains(p.first, p.second);
    }
};

// 四叉树类
class QuadTree {
public:
    std::string _name;      // 四叉树命名
    QuadTreeNode* _root;    // 根节点
    int _maxDepth;          // 树允许的最大深度
    int _maxDataNum;        // 树预设的单叶子节点内最大数据数量
    int _maxCurrDepth;      // 树当前的最大深度
    int _maxCurrDataNum;    // 树当前的单叶子节点的最大数据数量

public:
    /* 
    * brief 四叉树构造
    * rect:根节点的矩形空间
    * maxDepth:四叉树最大深度（根节点深度=1）
    * maxDataNum:四叉树叶子结点最大数据量
    */
    QuadTree(const Rect& rect, int maxDepth, int maxDataNum, const std::string& name = "QUANTREE")
        : _root(new QuadTreeNode(rect, 1)), _maxDepth(maxDepth), _maxDataNum(maxDataNum), _name(name){
        _maxCurrDepth = 1;
        _maxCurrDataNum = 0;
    }

    ~QuadTree() {
        delete _root;
    }

    // 禁止复制
    QuadTree(const QuadTree&) = delete;
    QuadTree& operator=(const QuadTree&) = delete;

    /*
    * brief 根据多边形的矩形包络框批量建立空间索引
    * poly_ptr:所有Polygon对象指针
    */
    bool CreatIndex(std::vector<Polygon*>& poly_ptr) {
        clear();

        // 赋值根节点数据
        _root->_datas = poly_ptr;

        // 递归划分
        return SplitNode(_root);
    }

    // 插入操作 TODO
    bool Insert() {}

    // 获取四叉树所有叶子结点数据
    void GetAllLeafData(std::vector<std::vector<Polygon*>>& outData) {
        GetAllLeafData(_root, outData);
    }

    // 获取某个坐标点所在的四叉树空间结点数据
    std::vector<Polygon*> GetLeafDataofPoint(Point& point) {
        QuadTreeNode* node = SearchNodeofPoint(_root, point);
        return node->_datas;
    }

    // for analysis 输出四叉树结点的所有矩形框
    void GetAllNodeRect(std::vector<Rect>& rects) {
        GetAllNodeRect(_root, rects);
    }

    // for analysis 输出四叉树叶子数
    int GetLeafNum() {
        return GetLeafNum(_root);
    }

private:
    // 清空操作
    void clear() { _root->clear(); }

    // 对节点进行递归划分
    bool SplitNode(QuadTreeNode* node)
    {
        if (node != nullptr)
        {
            // 节点的深度达到最大深度或者节点的数据量还不到最大阈值，无需拆分节点
            if (node->_depth >= _maxDepth){
                _maxCurrDataNum = static_cast<int>((node->_datas.size() > _maxCurrDataNum) ? node->_datas.size() : _maxCurrDataNum);
                return true;
            }
            if (node->_datas.size() <= _maxDataNum) {
                _maxCurrDataNum = static_cast<int>((node->_datas.size() > _maxCurrDataNum) ? node->_datas.size() : _maxCurrDataNum);
                return true;
            }
            _maxCurrDepth = ((node->_depth + 1 > _maxCurrDepth) ? node->_depth + 1 : _maxCurrDepth);

            // 生成四个子节点
            node->_divided = true;
            node->_lt = new QuadTreeNode(node->_rect.GetLTRect(), node->_depth + 1);
            node->_rt = new QuadTreeNode(node->_rect.GetRTRect(), node->_depth + 1);
            node->_lb = new QuadTreeNode(node->_rect.GetLBRect(), node->_depth + 1);
            node->_rb = new QuadTreeNode(node->_rect.GetRBRect(), node->_depth + 1);

            // 将数据分配给四个子节点
            for (size_t i = 0; i < node->_datas.size(); i++)
            {
                Polygon* ptr = node->_datas[i];
                const Rect& curr_rect = ptr->rect;
                // 根据矩形位置，分配到子节点中， 对于矩形在划分边界线上的情况， 它会被分配到多个其跨越的子节点中   
                if (node->_lt->_rect.Intersects(curr_rect)) node->_lt->_datas.push_back(ptr);
                if (node->_rt->_rect.Intersects(curr_rect)) node->_rt->_datas.push_back(ptr);
                if (node->_lb->_rect.Intersects(curr_rect)) node->_lb->_datas.push_back(ptr);
                if (node->_rb->_rect.Intersects(curr_rect)) node->_rb->_datas.push_back(ptr);
            }

            // 当前节点的数据需要清空
            node->_datas.clear();

            // 递归划分子节点
            SplitNode(node->_lt);
            SplitNode(node->_rt);
            SplitNode(node->_lb);
            SplitNode(node->_rb);

            return true;
        }
        return false;
    }

    // 获取四叉树所有叶子结点数据
    void GetAllLeafData(QuadTreeNode* node, std::vector<std::vector<Polygon*>>& outData) {
        if (!node) return;
        //叶子结点数据
        if (!node->_divided) {
            if(node->_datas.size() != 0)
                outData.push_back(node->_datas);
        }
        else{ 
            GetAllLeafData(node->_lt, outData);
            GetAllLeafData(node->_rt, outData);
            GetAllLeafData(node->_lb, outData);
            GetAllLeafData(node->_rb, outData);
        }
    }

    // 搜索某个坐标点所在的四叉树空间叶子结点,对于点位于边界上的情况,也只返回搜索到的第一个叶子结点
    QuadTreeNode* SearchNodeofPoint(QuadTreeNode* node, Point& point) {
        if (!node || !node->_rect.Contains(point.first, point.second)) return nullptr;
        
        // 预判点所在象限
        int xmid = (node->_rect._xmin + node->_rect._xmax) / 2;
        int ymid = (node->_rect._ymin + node->_rect._ymax) / 2;

        bool left = point.first <= xmid;
        bool top = point.second >= ymid;

        QuadTreeNode* child = nullptr;
        if (top) {
            child = left ? node->_lt : node->_rt;
        }
        else {
            child = left ? node->_lb : node->_rb;
        }

        if (child) {
            QuadTreeNode* res = SearchNodeofPoint(child, point);
            if (res) return res;
        }

        // 如果子节点不存在或不包含点，返回当前节点
        return node;
    }

    // for analysis 输出四叉树结点的所有矩形框
    void GetAllNodeRect(QuadTreeNode* node, std::vector<Rect>& rects) {
        if (!node) return;
        rects.push_back(node->_rect);
        GetAllNodeRect(node->_lt, rects);
        GetAllNodeRect(node->_rt, rects);
        GetAllNodeRect(node->_lb, rects);
        GetAllNodeRect(node->_rb, rects);
    }

    // for analysis 输出四叉树叶子数
    int GetLeafNum(QuadTreeNode* node) {
        if (!node) return 0;
        //叶子结点数据
        if (!node->_divided) {
            return 1;
        }
        int leaf_num = 0;
        leaf_num += GetLeafNum(node->_lt);
        leaf_num += GetLeafNum(node->_rt);
        leaf_num += GetLeafNum(node->_lb);
        leaf_num += GetLeafNum(node->_rb);
        return leaf_num;
    }
};