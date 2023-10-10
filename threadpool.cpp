#include "threadpool.h"

#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60; // 单位：秒

// 线程池构造
ThreadPool::ThreadPool()
	: initThreadSize_(0)
	, taskSize_(0)
	, idleThreadSize_(0)
	, curThreadSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning_(false)

{}

ThreadPool::~ThreadPool()
{
    isPoolRunning_ = false;

    std::unique_lock<std::mutex> lock(taskQueMtx_);
    notEmpty_.notify_all();
    exitCond_.wait(lock, [&]()->bool{return taskSize_ == 0;});

}

// 设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode){
    if(checkRunningState())
        return;
    poolMode_ = mode;
}

// 设置task任务队列上线阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshhold){
    if(checkRunningState())
        return;
    taskQueMaxThreshHold_ = threshhold;
}

// 设置线程池cached模式下线程阈值
void ThreadPool::setThreadSizeThreshHold(int threshhold){
    if(checkRunningState())
        return;
    threadSizeThreshHold_ = threshhold;
}

Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
    std::unique_lock<std::mutex> lock(taskQueMtx_);

    if(!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool{return taskQue_.size() < taskQueMaxThreshHold_;}))
    {

        // 表示notFull_等待1s，条件依然没有满足
		std::cerr << "task queue is full, submit task fail." << std::endl;
		// return task->getResult();  // Task  Result   线程执行完task，task对象就被析构掉了

        return Result(sp, false);
    }

    taskQue_.emplace(sp);
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
    return Result(sp);
}

void ThreadPool::start(int initThreadSize)
{
    // 设置线程池的运行状态
	isPoolRunning_ = true;

	// 记录初始线程个数
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

    for(int i = 0; i < initThreadSize; i ++){
        std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
    }

    for(int i = 0; i < initThreadSize; i ++){
        threads_[i]->start();
        idleThreadSize_++;
    }
}
void ThreadPool::threadFunc(int threadId)
{
    auto lastTime = std::chrono::high_resolution_clock().now();

    while(isPoolRunning_)
    {
        std::shared_ptr<Task> t;
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_);

            std::cout << "tid:" << std::this_thread::get_id()
				<< "尝试获取任务..." << std::endl;

            while(isPoolRunning_ && taskSize_ == 0)
            {
                if(poolMode_ == PoolMode::MODE_CACHED)
                {
                    if(std::cv_status::timeout ==
						notEmpty_.wait_for(lock, std::chrono::seconds(1)))
                        {
                            auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                            if(dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ >initThreadSize_)
                            {
                                threads_.erase(threadId);
                                curThreadSize_--;
							    idleThreadSize_--;

                                std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
                                    << std::endl;
                                return;

                            }
                        }
                }
                else
                {
                    notEmpty_.wait(lock);
                }
                // if(!isPoolRunning_)
                // {
                //     threads_.erase(threadId);
                //     exitCond_.notify_all();
                //     return;
                // }
                if(!isPoolRunning_)
                {
                    break;
                }
            }
 



            idleThreadSize_--;
            std::cout << "tid:" << std::this_thread::get_id()
				<< "获取任务成功..." << std::endl;
            t = taskQue_.front();
            taskQue_.pop();
            taskSize_--;

            if(taskQue_.size() > 0)
            {
                notEmpty_.notify_all();
            }
            notFull_.notify_all();
        }

        if(t != nullptr)
        {
            // t->run();
            t->exec();
        }

        idleThreadSize_++;
        lastTime = std::chrono::high_resolution_clock().now();

    }
    threads_.erase(threadId);
    exitCond_.notify_all();
}

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}
////////////////  线程方法实现
Task::Task() 
    : result_(nullptr)
    {}

void Task::exec()
{
    result_->setVal(run());
}
void Task::setResult(Result* result)
{   
    this->result_ = result;
}
////////////////  线程方法实现
int Thread::generateId_ = 0;

// 线程构造
Thread::Thread(ThreadFunc func)
	: func_(func)
    , threadId_(generateId_++)
{}

// 线程析构
Thread::~Thread() {}

void Thread::start()
{
    // 创建一个线程来执行一个线程函数 pthread_create
	std::thread t(func_, threadId_);  // C++11来说 线程对象t  和线程函数func_
	t.detach(); // 设置分离线程   pthread_detach  pthread_t设置成分离线程
}

int Thread::getId() const
{
    return threadId_;
}
////////////////  Result方法实现
Result::Result(std::shared_ptr<Task> task, bool isValid) 
    : task_(task), isValid_(isValid)
    {
        task_->setResult(this);
    }

void Result::setVal(Any any)
{
    any_ = std::move(any);
    sem_.post();
}
Any Result::get()
{
    if(isValid_ == false)
    {
        return "";
    }

    sem_.wait();
    return std::move(any_);

}