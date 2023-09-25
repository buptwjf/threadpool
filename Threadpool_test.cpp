// 线程池的头文件

#include "Threadpool.h"
#include "chrono"
#include "thread"
#include "iostream"


//class MyTask : public Task {
//public:
//    void run() override {
//        std::cout << std::this_thread::get_id() << " Begin task" << std::endl;
//        std::this_thread::sleep_for(std::chrono::seconds(5));
//    }
//};

/*
 * 有些场景，是希望能获取线程执行任务得返回值
 * 举例：
 * 1 + ... + 30000 的和
 * thread1 1 + ... + 10000
 * thread1 2 + ... + 20000
 * thread1 3 + ... + 30000
 * main thread：给每个线程分配计算空间，并等待他们算完，合并最终条件
 * */

class MyTask : public Task {
public:
    MyTask(int begin, int end) : begin_(begin), end_(end) {}

    // 问题一：怎么设计 run 函数的返回值，可以表示任意的类型
    // Java Python Object 是所有类型的基类，但是C++中没有这个东西
    // C++ 17 里面有 Any 类型，
    Any run() override { // 这里应该返回值为 int
        std::cout << "tid:" << std::this_thread::get_id() << " begin!" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        int sum = 0;
        for (int i = begin_; i <= end_; i++) {
            sum += i;
        }
        std::cout << "sum = " << sum << std::endl;
        std::cout << "tid:" << std::this_thread::get_id() << " end!" << std::endl;

        return sum;
    }

private:
    int begin_;
    int end_;
};

int main() {
    Threadpool pool;
    // 让用户设置线程池的工作模式
    pool.setMode(PoolMode::MODE_CACHED);
    // 开始启动线程池
    pool.start(4);



//    // 希望调用任务都能有返回值。如何设计这个 Result 机制
    Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100));
    Result res2 = pool.submitTask(std::make_shared<MyTask>(1, 100));
    Result res3 = pool.submitTask(std::make_shared<MyTask>(1, 100));
    Result res4 = pool.submitTask(std::make_shared<MyTask>(1, 100));
    Result res5 = pool.submitTask(std::make_shared<MyTask>(1, 100));
    Result res6 = pool.submitTask(std::make_shared<MyTask>(1, 100));

    int sum1 = res1.get().cast_<int>(); // get 返回一个 Any 类型，怎么转化成具体的类型
    int sum2 = res2.get().cast_<int>(); // get 返回一个 Any 类型，怎么转化成具体的类型
    int sum3 = res3.get().cast_<int>(); // get 返回一个 Any 类型，怎么转化成具体的类型
    std::cout << sum1 << std::endl;
    std::cout << sum2 << std::endl;
    std::cout << sum3 << std::endl;

    std::cout << "sum ===" << sum1 + sum2 + sum3 << std::endl;
    // Master - Slave 线程
    // Master 任务来分解任务，然后给各个 Slave 线程分配执行任务
    // 等待各个Slave 线程执行任务，返回结果
    // Master 线程合并各个任务结果，返回最终结果
    // 有四个线程
    getchar();
//    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}