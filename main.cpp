#include <iostream>
#include <chrono>
#include <thread>

#include "threadpool.h"

using uLong = unsigned long long;

class myTask : public Task
{
public:
    myTask(int begin, int end) 
    : begin_(begin)
    , end_(end)
    {}
    Any run()
    {
        uLong sum = 0;
        // std::this_thread::sleep_for(std::chrono::seconds(3));
        // for(int i = begin_; i <= end_; i++)
        // {
        //     sum += i;
        // }
        std::cout<<"done"<<std::endl;
        return sum;
    }
private:
    int begin_, end_;
};

int main(){
    
    ThreadPool myPool;
    // myPool.setMode(PoolMode::MODE_CACHED);
    myPool.start(4);

    Result res1 = myPool.submitTask(std::make_shared<myTask>(1, 100));
    Result res2 = myPool.submitTask(std::make_shared<myTask>(101, 200));
    Result res3 = myPool.submitTask(std::make_shared<myTask>(201, 300));
    myPool.submitTask(std::make_shared<myTask>(201, 3000));
    myPool.submitTask(std::make_shared<myTask>(201, 300));
    myPool.submitTask(std::make_shared<myTask>(201, 300));
    myPool.submitTask(std::make_shared<myTask>(201, 300));
    // uLong sum1 = res1.get().cast_<uLong>();
    // uLong sum2 = res2.get().cast_<uLong>();
    // uLong sum3 = res3.get().cast_<uLong>();

    // uLong sum = 0;
    // for(int i = 1; i <= 300; i ++){
    //     sum += i;
    // }0));
    // myPool.submitTask(std::make_shared<myTask>(201, 300));
    // myPool.submitTask(std::make_shared<myTask>(201, 3

    // std::cout<<(sum == (sum1+sum2+sum3));
    getchar();
    

    return 0;
}