#pragma once
#include <vector>
#include <iostream>
#include <mutex>
#include <cstddef>  // for std::max_align_t
#include <cstdint>  // for uint8_t

namespace MBSO {

    /**
     * @brief 高性能内存池模板类
     *
     * 该类实现了一个高性能的内存池，专门用于高效分配和释放特定类型的对象。
     * 设计目标是在高频分配/释放场景下提供比标准new/delete更优的性能。
     *
     * @tparam T 内存池管理的对象类型
     */
    template <typename T>
    class HighPerfMemoryPool {
    private:
        /**
         * @brief 链表节点结构（存储在空闲内存中）
         *
         * 这个结构用于实现自由链表，存储在空闲内存块中。
         * 一块内存两用，当内存块空闲时，它被用作链表节点；当分配给对象时，它被用作对象存储。
         * 这种设计也就是说内存归还时须析构对象，并将对象指针重新解析成Node指针，对于pod类型对象，构造析构开销几乎可以忽略不计，完全不用担心
         */
        struct Node {
            Node* next;  // 指向下一个空闲节点的指针
        };

        /**
         * @brief 内存块结构
         *
         * 表示一块连续分配的内存区域，包含多个可以分配的对象空间。
         */
        struct MemoryBlock {
            uint8_t* memory;  ///< 指向分配的内存块的指针
            size_t capacity;  ///< 内存块的总容量（字节）
            size_t used;      ///< 已使用的字节数（用于调试）

            /**
             * @brief 构造函数
             * @param size 要分配的内存大小（字节）
             */
            MemoryBlock(size_t size)
                : memory(static_cast<uint8_t*>(malloc(size))),
                capacity(size),
                used(0) {
            }

            /**
             * @brief 析构函数
             *
             * 释放分配的内存块。
             */
            ~MemoryBlock() {
                if (memory) {
                    free(memory);
                }
            }

            // 禁止拷贝（确保内存块不会被意外复制）
            MemoryBlock(const MemoryBlock&) = delete;
            MemoryBlock& operator=(const MemoryBlock&) = delete;

            /**
             * @brief 移动构造函数
             *
             * 允许内存块的所有权转移，用于std::vector扩容时的高效移动。
             *
             * @param other 要移动的源对象
             */
            MemoryBlock(MemoryBlock&& other) noexcept
                : memory(other.memory),
                capacity(other.capacity),
                used(other.used)
            {
                // 将源对象置为空状态，防止双重释放
                other.memory = nullptr;
                other.capacity = 0;
                other.used = 0;
            }

            /**
             * @brief 移动赋值运算符
             *
             * 允许内存块的所有权转移赋值。
             *
             * @param other 要移动的源对象
             * @return MemoryBlock& 返回当前对象的引用
             */
            MemoryBlock& operator=(MemoryBlock&& other) noexcept {
                if (this != &other) {
                    // 释放当前对象的内存
                    if (memory) {
                        free(memory);
                    }

                    // 接管源对象的内存
                    memory = other.memory;
                    capacity = other.capacity;
                    used = other.used;

                    // 将源对象置为空状态
                    other.memory = nullptr;
                    other.capacity = 0;
                    other.used = 0;
                }
                return *this;
            }
        };

        // 常量定义
        static constexpr size_t MIN_BLOCK_SIZE = 64 * 1024;  // 最小内存块大小（64KB）
        static constexpr size_t ALIGNMENT = alignof(std::max_align_t);  // 内存对齐要求
        static constexpr size_t LOCAL_CACHE_POOL_THRESHOLD = static_cast<size_t>(-1);  // 禁用：thread_local batching 在单线程和大数据集下均不如无竞争 mutex
        static constexpr size_t LOCAL_CACHE_REFILL_COUNT = 256;

        // 成员变量
        std::vector<MemoryBlock> blocks_;  // 存储所有内存块的容器
        Node* free_list_ = nullptr;        // 自由链表头指针（指向第一个空闲节点）
        size_t element_size_;              // 对齐后的单个元素大小（字节）
        size_t block_size_;                // 用户指定的内存块大小（字节）
        size_t elements_per_block_;        // 每个内存块可容纳的元素数量
        size_t total_allocated_ = 0;       // 小池总共分配的对象计数
        size_t total_freed_ = 0;           // 小池总共释放的对象计数
        mutable std::mutex mutex_;         // 大池全局自由链表互斥锁

        /**
         * @brief 计算对齐后的元素大小
         *
         * 确保元素大小满足内存对齐要求，并至少能容纳Node结构。
         *
         * @return 对齐后的元素大小（字节）
         */
        size_t aligned_element_size() const {
            // 取T和Node中较大的大小作为基础大小
            const size_t basic_size = sizeof(T) > sizeof(Node) ? sizeof(T) : sizeof(Node);
            // 计算对齐后的大小(向上对齐到指定边界的技术,核心逻辑是通过位运算将基础大小 basic_size 向上舍入到 ALIGNMENT 的倍数)
            return (basic_size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
        }

        /**
         * @brief 添加新内存块
         *
         * 当自由链表为空时调用此方法分配新内存块。
         * 新内存块会被划分为多个元素大小的块并加入自由链表。
         */
        void add_block() {
            // 计算新内存块的大小
            size_t new_block_size = block_size_;

            // 如果用户指定的元素数量要求更大的内存块
            if (elements_per_block_ * aligned_element_size() > new_block_size) {
                new_block_size = elements_per_block_ * aligned_element_size();
            }

            // 确保内存块不小于最小大小
            if (new_block_size < MIN_BLOCK_SIZE) {
                new_block_size = MIN_BLOCK_SIZE;
            }

            // 创建新内存块并添加到blocks_中
            blocks_.emplace_back(new_block_size);
            MemoryBlock& block = blocks_.back();

            // 计算块中可容纳的元素数量
            const size_t chunk_size = aligned_element_size();
            const size_t num_chunks = block.capacity / chunk_size;

            // 将新内存块的所有元素加入自由链表
            for (size_t i = 0; i < num_chunks; ++i) {
                // 将内存位置解释为Node指针
                Node* node = reinterpret_cast<Node*>(block.memory + i * chunk_size);
                // 将新节点插入链表头部
                node->next = free_list_;
                free_list_ = node;
            }
        }

        bool useLocalCache() const {
            return elements_per_block_ >= LOCAL_CACHE_POOL_THRESHOLD;
        }

        void refillLocalCache(std::vector<Node*>& cache) {
            std::lock_guard<std::mutex> lock(mutex_);
            while (!free_list_) {
                add_block();
            }

            for (size_t i = 0; i < LOCAL_CACHE_REFILL_COUNT && free_list_; ++i) {
                Node* node = free_list_;
                free_list_ = free_list_->next;
                cache.push_back(node);
            }
        }

        // 将线程本地释放缓存批量归还到全局自由链表
        void flushLocalFreeCache(std::vector<Node*>& free_cache) {
            if (free_cache.empty()) return;
            std::lock_guard<std::mutex> lock(mutex_);
            for (Node* node : free_cache) {
                node->next = free_list_;
                free_list_ = node;
            }
            free_cache.clear();
        }

    public:
        /**
         * @brief 构造函数
         *
         * @param elements_per_block 每个内存块的目标元素数量
         * @param block_size 内存块大小（字节），0表示自动计算
         */
        HighPerfMemoryPool(size_t elements_per_block = 1024, size_t block_size = 0)
            : element_size_(aligned_element_size()),  // 计算对齐后的元素大小
            block_size_(block_size),
            elements_per_block_(elements_per_block)
        {
            // 预分配第一个内存块
            add_block();
        }

        /**
         * @brief 析构函数
         *
         * 检查是否有内存泄漏并输出警告。
         */
        ~HighPerfMemoryPool() {
            // 检查是否有未释放的对象
            const size_t allocated = total_allocated();
            const size_t freed = total_freed();
            if (allocated != freed) {
                std::cerr << "WARNING: Memory leak detected: "
                    << allocated - freed
                    << " objects not freed\n";
            }
        }

        // 禁止拷贝（内存池通常不应被复制）
        HighPerfMemoryPool(const HighPerfMemoryPool&) = delete;
        HighPerfMemoryPool& operator=(const HighPerfMemoryPool&) = delete;

        /**
         * @brief 分配并构造对象
         *
         * 从内存池中分配内存并在该内存上构造对象。
         *
         * @tparam Args 构造函数的参数类型
         * @param args 构造函数的参数
         * @return T* 指向新创建对象的指针
         */
        template <typename... Args>
        T* newElement(Args&&... args) {
            Node* node = nullptr;

            if (useLocalCache()) {
                thread_local std::vector<Node*> local_cache;
                thread_local const HighPerfMemoryPool<T>* local_owner = nullptr;

                if (local_owner != this) {
                    local_cache.clear();
                    local_owner = this;
                }
                if (local_cache.empty()) {
                    refillLocalCache(local_cache);
                }
                node = local_cache.back();
                local_cache.pop_back();
            } else {
                std::lock_guard<std::mutex> lock(mutex_);
                // 确保自由链表不为空（必要时分配新内存块）
                if (!free_list_) {
                    add_block();
                }
                node = free_list_;
                free_list_ = free_list_->next;
                ++total_allocated_;
            }

            // 使用placement new在获取的内存上构造对象
            T* object = new (node) T(std::forward<Args>(args)...);

            return object;
        }

        /**
         * @brief 销毁对象并回收内存
         *
         * 调用对象的析构函数并将内存归还给自由链表。
         *
         * @param object 要销毁的对象指针
         */
        void pushReuseElement(T* object) {
            if (!object) return;  // 空指针检查

            // 调用对象的析构函数
            object->~T();

            // 将对象内存重新解释为Node指针
            Node* node = reinterpret_cast<Node*>(object);

            if (useLocalCache()) {
                // 大池：释放路径也采用线程本地批量缓存，每攒满 LOCAL_CACHE_REFILL_COUNT
                // 个节点才获取一次 mutex 批量归还，避免高频锁竞争。
                thread_local std::vector<Node*> local_free_cache;
                thread_local std::mutex* local_free_mutex = nullptr;
                thread_local Node** local_free_list_ptr = nullptr;

                if (local_free_mutex != &mutex_) {
                    // 切换到不同池实例：先刷新旧池的缓存
                    if (local_free_mutex && !local_free_cache.empty()) {
                        std::lock_guard<std::mutex> lock(*local_free_mutex);
                        for (Node* n : local_free_cache) {
                            n->next = *local_free_list_ptr;
                            *local_free_list_ptr = n;
                        }
                        local_free_cache.clear();
                    }
                    local_free_mutex = &mutex_;
                    local_free_list_ptr = &free_list_;
                }

                local_free_cache.push_back(node);
                if (local_free_cache.size() >= LOCAL_CACHE_REFILL_COUNT) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    for (Node* n : local_free_cache) {
                        n->next = free_list_;
                        free_list_ = n;
                    }
                    local_free_cache.clear();
                }
            } else {
                std::lock_guard<std::mutex> lock(mutex_);
                node->next = free_list_;
                free_list_ = node;
                ++total_freed_;
            }
        }

        // ================= 统计信息方法 =================

        /**
         * @brief 获取内存块数量
         * @return 已分配的内存块数量
         */
        size_t block_count() const { return blocks_.size(); }

        /**
         * @brief 获取总分配对象数
         * @return 总共分配的对象数量
         */
        size_t total_allocated() const { return total_allocated_; }

        /**
         * @brief 获取总释放对象数
         * @return 总共释放的对象数量
         */
        size_t total_freed() const { return total_freed_; }

        /**
         * @brief 获取活跃对象数
         * @return 当前活跃（已分配但未释放）的对象数量
         */
        size_t active_objects() const { return total_allocated() - total_freed(); }

        /**
         * @brief 获取自由链表大小
         * @return 自由链表中空闲节点的数量
         */
        size_t free_list_size() const {
            std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
            if (useLocalCache()) {
                lock.lock();
            }
            size_t count = 0;
            Node* node = free_list_;

            // 遍历自由链表计数节点
            while (node) {
                count++;
                node = node->next;
            }
            return count;
        }
    };

}