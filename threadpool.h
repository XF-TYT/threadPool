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
class Any
{
public:
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;

    template<typename T>
    Any(T data) : base_(std::make_unique<Derive<T>> (data)){};

    template<typename T>
    T cast_()
    {
        Derive<T>* d = dynamic_cast<Derive<T>*>(base_.get());
        if(d == nullptr)
        {
            throw "type is unmatch!";
        }
        return d->data_;
    }
private:
    class Base
    {
    public:
        virtual ~Base() = default;
    };

    template<typename T>
    class Derive : public Base
    {
    public:
        Derive(T data) : data_(data){}
        T data_;
    };

    std::unique_ptr<Base> base_;
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

class Result;

class Task{
public:
    Task();
    ~Task() = default;
    virtual Any run() = 0;
    void exec();
    void setResult(Result* result);
private:
    Result* result_;
};

class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	void setVal(Any any);

	Any get();   
private:
	Any any_; // 存储任务的返回值
	Semaphore sem_; // 线程通信信号量
	std::shared_ptr<Task> task_; //指向对应获取返回值的任务对象 
	std::atomic_bool isValid_; // 返回值是否有效

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
	Result submitTask(std::shared_ptr<Task> sp);

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

    std::queue<std::shared_ptr<Task>> taskQue_;
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