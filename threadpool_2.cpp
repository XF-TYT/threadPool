#include "threadpool_2.h"

#include <thread>
#include <iostream>
#include <future>

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

    for(;;)
    {
        std::function<void()> t;
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_);

            std::cout << "tid:" << std::this_thread::get_id()
				<< "尝试获取任务..." << std::endl;

            while(taskSize_ == 0)
            {
                // 回收线程资源
                if (!isPoolRunning_)
                {
                    threads_.erase(threadId); // std::this_thread::getid()
                    std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
                        << std::endl;
                    exitCond_.notify_all();
                    return; // 线程函数结束，线程结束
                }
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
            // t->exec();
            t();
        }

        idleThreadSize_++;
        lastTime = std::chrono::high_resolution_clock().now();

    }
}

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
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
