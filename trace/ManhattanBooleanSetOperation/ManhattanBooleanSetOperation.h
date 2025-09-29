#pragma once
/* \file  通用接口 */

#include "./solve/Overlay.h"
#include <vector>

namespace MBSO {
    using std::vector;
    using std::pair;

    using myPolygon = vector<MPoint_2>;
    using myPolygonSet = vector<myPolygon>;

    // 通用接口
    // a 并 b 请确保输入参数中每个多边形的点是顺时针顺序, 然后输出是顺时针的
    myPolygonSet join(const myPolygon& a, const  myPolygon& b);
    myPolygonSet join(const myPolygonSet& a, const myPolygon& b);
    myPolygonSet join(const myPolygon& a, const myPolygonSet& b);
    myPolygonSet join(const myPolygonSet& a, const myPolygonSet& b);

    // a 交 b 请确保输入参数中每个多边形的点是顺时针顺序, 然后输出是顺时针的
    myPolygonSet intersect(const myPolygon& a, const myPolygon& b);
    myPolygonSet intersect(const myPolygonSet& a, const myPolygon& b);
    myPolygonSet intersect(const myPolygon& a, const myPolygonSet& b);
    myPolygonSet intersect(const myPolygonSet& a, const myPolygonSet& b);

    // a 差 b 请确保输入参数中每个多边形的点是顺时针顺序, 然后输出是顺时针的
    myPolygonSet difference(const myPolygon& a, const myPolygon& b);
    myPolygonSet difference(const myPolygonSet& a, const myPolygon& b);
    myPolygonSet difference(const myPolygon& a, const myPolygonSet& b);
    myPolygonSet difference(const myPolygonSet& a, const myPolygonSet& b);


    //========================= 提取解实现 ========================
    myPolygonSet getResult(MPolygonSet* res) {
        myPolygonSet ans;
        int polygonSize = res->getMPolygonSize();
        ans.resize(polygonSize);
        // 提取每个多边形
        for (int i = 0; i < polygonSize; ++i) {
            auto& ansPolygon = ans[i];
            ansPolygon.reserve(res->mpolygons[i].edges.size());
            // 提取每个点
            for (auto& edgePtr : res->mpolygons[i].edges) {
                ansPolygon.emplace_back(edgePtr->ori->point);
            }
        }
        return ans;
    }

    //========================= 并实现 ========================
    myPolygonSet join(const myPolygon& a, const myPolygon& b) {
        myPolygonSet aset, bset;
        aset.push_back(a);
        bset.push_back(b);
        return join(aset, bset);
    }
    myPolygonSet join(const myPolygonSet& a, const myPolygon& b) {
        myPolygonSet bset;
        bset.push_back(b);
        return join(a, bset);
    }
    myPolygonSet join(const myPolygon& a, const myPolygonSet& b) {
        return join(b, a);
    }
    myPolygonSet join(const myPolygonSet& a, const myPolygonSet& b) {
        Overlay overlay;
        MPolygonSet mps1(a);
        MPolygonSet mps2(b);
        MPolygonSet res;
        overlay.solve(&mps1, &mps2, &res, MBSO::OP_TYPE::UNION);
        return getResult(&res);
    }

    //========================= 交实现 ========================
    myPolygonSet intersect(const myPolygon& a, const myPolygon& b) {
        myPolygonSet aset, bset;
        aset.push_back(a);
        bset.push_back(b);
        return intersect(aset, bset);
    }
    myPolygonSet intersect(const myPolygonSet& a, const myPolygon& b) {
        myPolygonSet bset;
        bset.push_back(b);
        return intersect(a, bset);
    }
    myPolygonSet intersect(const myPolygon& a, const myPolygonSet& b) {
        return intersect(b, a);
    }
    myPolygonSet intersect(const myPolygonSet& a, const myPolygonSet& b) {
        Overlay overlay;
        MPolygonSet mps1(a);
        MPolygonSet mps2(b);
        MPolygonSet res;
        overlay.solve(&mps1, &mps2, &res, MBSO::OP_TYPE::INTER);
        return getResult(&res);
    }

    //========================= 差实现 ========================
    myPolygonSet difference(const myPolygon& a, const myPolygon& b) {
        myPolygonSet aset, bset;
        aset.push_back(a);
        bset.push_back(b);
        return difference(aset, bset);
    }
    myPolygonSet difference(const myPolygonSet& a, const myPolygon& b) {
        myPolygonSet bset;
        bset.push_back(b);
        return difference(a, bset);
    }
    myPolygonSet difference(const myPolygon& a, const myPolygonSet& b) {
        myPolygonSet aset;
        aset.push_back(a);
        return difference(aset, b);
    }
    myPolygonSet difference(const myPolygonSet& a, const myPolygonSet& b) {
        Overlay overlay;
        MPolygonSet mps1(a);
        MPolygonSet mps2(b);
        MPolygonSet res;
        overlay.solve(&mps1, &mps2, &res, MBSO::OP_TYPE::DIFF);
        return getResult(&res);
    }

} // namespace MBSO