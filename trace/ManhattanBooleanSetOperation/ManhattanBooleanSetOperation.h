#pragma once
/* \file  通用接口 */

#include "./model/MPolygonSet.h"
#include "./solve/Overlay.h"
#include <vector>
using namespace MBSO;
using std::vector;
using std::pair;

using myPolygon = vector<MPoint_2>;
using myPolygonSet = vector<myPolygon>;

// 通用接口
// a 并 b
myPolygonSet join(const myPolygon &a,const  myPolygon &b);
myPolygonSet join(const myPolygonSet &a, const myPolygon &b);
myPolygonSet join(const myPolygon &a, const myPolygonSet &b);
myPolygonSet join(const myPolygonSet &a, const myPolygonSet &b);

// a 交 b
myPolygonSet intersect(const myPolygon &a, const myPolygon &b);
myPolygonSet intersect(const myPolygonSet &a, const myPolygon &b);
myPolygonSet intersect(const myPolygon &a, const myPolygonSet &b);
myPolygonSet intersect(const myPolygonSet &a, const myPolygonSet &b);

// a 差 b
myPolygonSet difference(const myPolygon &a, const myPolygon &b);
myPolygonSet difference(const myPolygonSet &a, const myPolygon &b);
myPolygonSet difference(const myPolygon &a, const myPolygonSet &b);
myPolygonSet difference(const myPolygonSet &a, const myPolygonSet &b);


//========================= 提取解实现 ========================
myPolygonSet getResult(MPolygonSet* res){
    return myPolygonSet();
}

//========================= 并实现 ========================
myPolygonSet join(const myPolygon &a, const myPolygon &b){
    myPolygonSet aset, bset;
    aset.push_back(a);
    bset.push_back(b);
    return join(aset, bset);
}
myPolygonSet join(const myPolygonSet &a, const myPolygon &b){
    myPolygonSet bset;
    bset.push_back(b);
    return join(a, bset);
}
myPolygonSet join(const myPolygon &a, const myPolygonSet &b){
    return join(b, a);
}
myPolygonSet join(const myPolygonSet &a, const myPolygonSet &b){
    Overlay overlay;
    MPolygonSet mps1(a);
    MPolygonSet mps2(b);
    MPolygonSet res;
    overlay.solve(&mps1, &mps2, &res, MBSO::OP_TYPE::UNION);
    return getResult(&res);
}

//========================= 交实现 ========================
myPolygonSet intersect(const myPolygon &a, const myPolygon &b){
    myPolygonSet aset, bset;
    aset.push_back(a);
    bset.push_back(b);
    return intersect(aset, bset);
}
myPolygonSet intersect(const myPolygonSet &a, const myPolygon &b){
    myPolygonSet bset;
    bset.push_back(b);
    return intersect(a, bset);
}
myPolygonSet intersect(const myPolygon &a, const myPolygonSet &b){
    return intersect(b, a);
}
myPolygonSet intersect(const myPolygonSet &a, const myPolygonSet &b){
    Overlay overlay;
    MPolygonSet mps1(a);
    MPolygonSet mps2(b);
    MPolygonSet res;
    overlay.solve(&mps1, &mps2, &res, MBSO::OP_TYPE::INTER);
    return getResult(&res);
}

//========================= 差实现 ========================
myPolygonSet difference(const myPolygon &a, const myPolygon &b){
    myPolygonSet aset, bset;
    aset.push_back(a);
    bset.push_back(b);
    return difference(aset, bset);
}
myPolygonSet difference(const myPolygonSet &a, const myPolygon &b){
    myPolygonSet bset;
    bset.push_back(b);
    return difference(a, bset);
}
myPolygonSet difference(const myPolygon &a, const myPolygonSet &b){
    myPolygonSet aset;
    aset.push_back(a);
    return difference(aset, b);
}
myPolygonSet difference(const myPolygonSet &a, const myPolygonSet &b){
    Overlay overlay;
    MPolygonSet mps1(a);
    MPolygonSet mps2(b);
    MPolygonSet res;
    overlay.solve(&mps1, &mps2, &res, MBSO::OP_TYPE::DIFF);
    return getResult(&res);
}