/*
	Copyright(c) 2012 Jakob Progsch, Václav Zeman

	This software is provided 'as-is', without any express or implied
	warranty.In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions :

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software.If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.

	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.

	3. This notice may not be removed or altered from any source
	distribution.
*/

#ifndef CN_HUST_THREAD_POOL_H
#define CN_HUST_THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <type_traits>

namespace goal {
	class NaiveThreadPool {
	public:
		NaiveThreadPool(size_t);
		template<class F, class ... Args>
		auto enqueue(F&& f, Args&&... args)
			//-> std::future<typename std::invoke_result<F, Args...>::type>;
			-> std::future<
#if defined(__cpp_lib_is_invocable) && __cpp_lib_is_invocable >= 201703
			typename std::invoke_result<F&&, Args&&...>::type
#else
			typename std::result_of<F && (Args&&...)>::type
#endif
			>;
		// 添加同步等待函数
        void wait();
		
		~NaiveThreadPool();
	private:
		// need to keep track of threads so we can join them
		std::vector< std::thread > workers;
		// the task queue
		std::queue< std::function<void()> > tasks;

		// synchronization
		std::mutex queue_mutex;					// 任务队列互斥量，用于防止数据竞争
		std::condition_variable condition;		// 任务队列条件变量，用于实现线程间的同步和通信，避免忙等待
		std::condition_variable completion_condition; // 完成条件变量
		bool stop;

		// 任务计数器
        std::atomic<size_t> active_tasks{0};
	};

	// the constructor just launches some amount of workers
	inline NaiveThreadPool::NaiveThreadPool(size_t threads)
		: stop(false)
	{
		for (size_t i = 0; i < threads; ++i)
			workers.emplace_back(
				[this]
				{
					for (;;) // 每个工作线程无限循环处理任务
					{
						std::function<void()> task;

						{
							/* 临界区 */
							std::unique_lock<std::mutex> lock(this->queue_mutex);		// 使用锁包装器自动管理锁生命周期，使用 unique_lock 用于与条件变量配合
							this->condition.wait(lock,
								[this] { return this->stop || !this->tasks.empty(); });	// 使用条件等待，若谓词条件满足（收到停止信号或任务队列非空）则继续执行；
																						// 否则线程进入阻塞等待状态，等待其他线程调用notify_one()或notify_all()唤醒，唤醒时重新检查条件，防止虚假唤醒。
																						// 若进入等待，则会自动释放锁，唤醒后重新获取, 只有 unique_lock 支持在等待时解锁和重新锁定
							if (this->stop && this->tasks.empty())
								return;
							task = std::move(this->tasks.front());
							this->tasks.pop();
							
                            ++active_tasks; // 增加活动任务计数
							/* 退出临界区自动释放锁 */
						}

						task();	// 任务执行

						// 减少活动任务计数
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            --active_tasks;
                            
                            // 如果没有活动任务，通知等待线程
                            if (active_tasks == 0 && tasks.empty()) {
                                completion_condition.notify_all();
                            }
                        }
					}
				}
		);
	}

	// add new work item to the pool
	template<class F, class ... Args>
	auto NaiveThreadPool::enqueue(F&& f, Args&&... args)
		//-> std::future<typename std::invoke_result<F, Args...>::type>
		-> std::future<
#if defined(__cpp_lib_is_invocable) && __cpp_lib_is_invocable >= 201703
		typename std::invoke_result<F&&, Args&&...>::type
#else
		typename std::result_of<F && (Args&&...)>::type
#endif
		>
	{
		//using return_type = typename std::invoke_result<F, Args...>::type;
#if defined(__cpp_lib_is_invocable) && __cpp_lib_is_invocable >= 201703
		using return_type = typename std::invoke_result<F&&, Args&&...>::type;
#else
		using return_type = typename std::result_of<F && (Args&&...)>::type;
#endif

		auto task = std::make_shared< std::packaged_task<return_type()> >(	// packaged_task 封装绑定后的可调用对象。使用智能指针shared_ptr提供对任务对象的智能生命周期管理和所有权共享
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)		// std::bind 将原始函数 f 和参数 args... 绑定在一起，创建一个新的可调用对象，该对象在被调用时不需要额外参数，使用完美转发保持参数的值类别
		);

		std::future<return_type> res = task->get_future();	// task->get_future() 返回一个与 packaged_task 关联的 std::future 对象,这个 future 对象提供了访问任务结果的接口.get()，get() 只能调用一次，若调用时任务未完成，则调用线程被阻塞直到完成
		{
			std::unique_lock<std::mutex> lock(queue_mutex);

			// don't allow enqueueing after stopping the pool
			if (stop)
				throw std::runtime_error("enqueue on stopped NaiveThreadPool");

			tasks.emplace([task]() { (*task)(); }); // lambda按值捕获了shared_ptr（即task），这样就会增加引用计数，保证在lambda执行时，task指向的对象仍然存在
		}
		condition.notify_one();	// 唤醒一个等待的工作线程，如果没有等待线程，则无操作
		return res;
	}

	// 同步等待函数实现
    inline void NaiveThreadPool::wait() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        completion_condition.wait(lock, [this] {
            // 等待条件：没有活动任务且任务队列为空
            return active_tasks == 0 && tasks.empty();
        });
    }

	// the destructor joins all threads
	inline NaiveThreadPool::~NaiveThreadPool()
	{
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			stop = true;
		}
		condition.notify_all();
		for (std::thread& worker : workers)
			worker.join(); // // 等待线程结束
	}
}
#endif
