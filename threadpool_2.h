#ifndef THREAPOOL_H
#define THREAPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <future>
#include <iostream>
class Semaphore
{
public:
    Semaphore(int limit = 0) 
        : resLimit(limit)
        {}
    ~Semaphore() = default;

    void wait(){
        std::unique_lock<std::mutex> lock(lock_);
        cond_.wait(lock, [&]()->bool{return resLimit > 0;});
        resLimit--;

    }

    void post(){
        std::unique_lock<std::mutex> lock(lock_);
        resLimit++;
        cond_.notify_all();
    }
private:
    int resLimit;
    std::mutex lock_;
    std::condition_variable cond_;
    
};
// 线程池支持的模式
enum class PoolMode
{
	MODE_FIXED,  // 固定数量的线程
	MODE_CACHED, // 线程数量可动态增长
};

class Thread{
public:
    using ThreadFunc = std::function<void(int)>;

	// 线程构造
	Thread(ThreadFunc func);
	// 线程析构
	~Thread();
    
    void start();

    	// 获取线程id
	int getId()const;
private:
    ThreadFunc func_;
    static int generateId_;
	int threadId_;  // 保存线程id
};

class ThreadPool
{
public:
    	// 线程池构造
	ThreadPool();

	// 线程池析构
	~ThreadPool();

	// 设置线程池的工作模式
	void setMode(PoolMode mode);

	// 设置task任务队列上线阈值
	void setTaskQueMaxThreshHold(int threshhold);

	// 设置线程池cached模式下线程阈值
	void setThreadSizeThreshHold(int threshhold);

	// 给线程池提交任务
    template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args)->std::future<decltype(func(args...))>
    {
    
        using returnType = decltype(func(args...));
        auto taskResult = std::make_shared<std::packaged_task<returnType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        std::future<returnType> result = taskResult->get_future();

        std::unique_lock<std::mutex> lock(taskQueMtx_);
        if(!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool{return taskQue_.size() < taskQueMaxThreshHold_;}))
        {
            // 表示notFull_等待1s，条件依然没有满足
            std::cerr << "task queue is full, submit task fail." << std::endl;
            // return task->getResult();  // Task  Result   线程执行完task，task对象就被析构掉了
            auto taskResult = std::make_shared<std::packaged_task<returnType()>>([]()->returnType{return returnType();});
            (*taskResult)();
            return taskResult->get_future();
        }

        taskQue_.emplace([taskResult](){(*taskResult)();});
        taskSize_++;
        

        notEmpty_.notify_all();

        if(poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize_ && curThreadSize_ < threadSizeThreshHold_)
        {
            std::cout << ">>> create new thread..." << std::endl;

            std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
            int threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr));
            threads_[threadId]->start();
            // 修改线程个数相关的变量
            curThreadSize_++;
            idleThreadSize_++;

        }
        return result;
    }

    void start(int initThreadSize = 4);

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// 定义线程函数
	void threadFunc(int threadId);

    bool checkRunningState() const;
private:
    std::unordered_map<int ,std::unique_ptr<Thread>> threads_;
    
    int initThreadSize_;  // 初始的线程数量
	size_t threadSizeThreshHold_; // 线程数量上限阈值
    std::atomic_int curThreadSize_;	// 记录当前线程池里面线程的总数量
	std::atomic_int idleThreadSize_; // 记录空闲线程的数量


    std::queue<std::function<void()>> taskQue_;
    std::atomic_uint taskSize_;
    size_t taskQueMaxThreshHold_;  // 任务队列数量上限阈值 
    
    std::mutex taskQueMtx_; // 保证任务队列的线程安全
	std::condition_variable notFull_; // 表示任务队列不满
	std::condition_variable notEmpty_; // 表示任务队列不空
    std::condition_variable exitCond_;

    PoolMode poolMode_;
    std::atomic_bool isPoolRunning_;



};

#endif

