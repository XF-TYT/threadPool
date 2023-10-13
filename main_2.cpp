#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include <chrono>
#include "threadpool_2.h"
using namespace std;
int fun(int a, int b)
{
    this_thread::sleep_for(chrono::seconds(3));
    return a + b;
}
int main(){
    
    ThreadPool myPool;
    myPool.setMode(PoolMode::MODE_CACHED);
    myPool.start(1);
    future<int> r1 = myPool.submitTask(fun,1,3);
    future<int> r2 = myPool.submitTask(fun,12,3);
    future<int> r3 = myPool.submitTask(fun,13,3);
    future<int> r4 = myPool.submitTask([](int a)->int{
        int sum = 0;
        for(int i = 1; i <= a;i++) sum+=i;
        return sum;
    },100);
    future<int> r6 = myPool.submitTask(fun,13,3);
    future<int> r9 = myPool.submitTask(fun,13,3);
    cout<<r1.get()<<endl;
    cout<<r2.get()<<endl;
    cout<<r3.get()<<endl;
    cout<<r4.get()<<endl;


    getchar();
    

    return 0;
}