#pragma once
#include <list>
#include <vector>
#include <iostream>

namespace MBSO {
    using std::vector;
    using std::list;

    template <class T>
    struct MemoryPool
    {
    private:
        vector<vector<T>> allElements;
        list<T*> reuseElements;
        int curPoolIndex;       // 当前是第多少块
        int curEleSize;         // 当前块遍历到第几个元素了
        int MAX_ELE_SIZE;       // 每块元素的大小
        int MAX_POOL_SIZE;      // 最多有多少块
    public:
        MemoryPool()
        {
            init(10000, 10);
        }
        MemoryPool(int _size, int _poolSize = 10)
        {
            init(_size, _poolSize);
        }
        void init(int _size, int _poolSize)
        {
            curPoolIndex = 0;
            curEleSize = 0;
            MAX_ELE_SIZE = _size * 2;
            MAX_POOL_SIZE = _poolSize;
            allElements.reserve(MAX_POOL_SIZE);
            allElements.emplace_back(vector<T>());
            allElements[curPoolIndex].resize(MAX_ELE_SIZE);
        }
        T* newElement(const T& p)
        {
            T* ptr = newElement();
            (*ptr) = p;
            return ptr;
        }

        T* newElement()
        {
            if (reuseElements.size() > 0) {
                T* tmp = reuseElements.front();
                reuseElements.pop_front();
                return tmp;
            }
            if (curEleSize == MAX_ELE_SIZE)
            {
                curPoolIndex++;
                if (curPoolIndex >= MAX_POOL_SIZE)
                {
                    throw std::runtime_error("MemoryPool is too small!");
                }
                std::cerr << "MemoryPool Expand Capacity" << std::endl;
                allElements.emplace_back(vector<T>());
                allElements[curPoolIndex].resize(MAX_ELE_SIZE);
                curEleSize = 0;
            }
            return &allElements[curPoolIndex][curEleSize++];
        }

        void pushReuseElement(T* t)
        {
            reuseElements.push_back(t);
        }

        int getMaxPoolSize()
        {
            return MAX_POOL_SIZE;
        }
        int getMaxEleSize()
        {
            return MAX_ELE_SIZE;
        }
        int getUsedPoolNum()
        {
            return curPoolIndex + 1;
        }
        long long getAllEleSize()
        {
            return 1ll * MAX_POOL_SIZE * MAX_ELE_SIZE;
        }
        long long getUsedEleNum()
        {
            return (1ll * curPoolIndex * MAX_ELE_SIZE) + curEleSize;
        }
    };
} // namespace MBSO
