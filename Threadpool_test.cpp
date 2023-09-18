// 线程池的头文件

#include "Threadpool.h"
#include "chrono"
#include "thread"
#include "iostream"


//class MyTask : public Task {
//public:
//    void run() {
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
    MyTask(int begin, int end) : begin_(begin), end_(end) { }

    // 问题一：怎么设计 run 函数的返回值，可以表示任意的类型
    // Java Python Object 是所有类型的基类，但是C++中没有这个东西
    // C++ 17 里面有 Any 类型，
    void run() { // 这里应该返回值为 int
        std::cout <<"tid:" << std::this_thread::get_id() << " begin!" << std::endl;
        int sum = 0;
        for(int i = begin_; i <=end_; i++){
            sum += 1;
        }
        std::cout <<"tid:" << std::this_thread::get_id() << " end!" << std::endl;
//        return sum;
    }
private:
    int begin_;
    int end_;
};

int main() {
    Threadpool pool;
    pool.start(4);

    // 问题二：如何设计这里的 Result 机制
//  Result res = pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    // 当有 10 个任务的时候，会有 8个任务成功，2个任务会失败

    // 有四个线程
    getchar();
//    std::this_thread::sleep_for(std::chrono::seconds(5));
}