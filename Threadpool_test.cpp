// 线程池的头文件

#include "Threadpool.h"
#include "chrono"
#include "thread"
#include "iostream"

using uLong = unsigned long long;

//class MyTask : public Task {
    class MyTask : public Task {
public:
    Any run() {
        std::cout << std::this_thread::get_id() << " Begin task" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return "";
    }
};

/*
 * 有些场景，是希望能获取线程执行任务得返回值
 * 举例：
 * 1 + ... + 30000 的和
 * thread1 1 + ... + 10000
 * thread1 2 + ... + 20000
 * thread1 3 + ... + 30000
 * main thread：给每个线程分配计算空间，并等待他们算完，合并最终条件
 * */

//class MyTask : public Task {
//public:
//    MyTask(int begin, int end) : begin_(begin), end_(end) {}
//
//    // 问题一：怎么设计 run 函数的返回值，可以表示任意的类型
//    // Java Python Object 是所有类型的基类，但是C++中没有这个东西
//    // C++ 17 里面有 Any 类型，
//    Any run() override { // 这里应该返回值为 int
//        std::cout << "tid:" << std::this_thread::get_id() << " begin!" << std::endl;
//        uLong sum = 0;
//        for (int i = begin_; i <= end_; i++) {
//            sum += i;
//        }
//        std::cout << "sum = " << sum << std::endl;
//        std::cout << "tid:" << std::this_thread::get_id() << " end!" << std::endl;
//        return sum;
//    }
//
//private:
//    int begin_;
//    int end_;
//};

int main() {
    Threadpool pool;
    pool.start(4);

    // 问题二：如何设计这里的 Result 机制，
//    return task->getResult(); // Task Result
//    这样不好，线程执行完 task，任务 task 就被析构掉，依赖于task 的 result 就没了
//  Result res = pool.submitTask(std::make_shared<MyTask>());

//    res.get().cast_<>;
// get 返回了一个 Any ，如何转换成具体的类型
//    当线程还没有把任务执行完，因此不能 get() 出来


//    Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 10000));
//    Result res2 = pool.submitTask(std::make_shared<MyTask>(10001, 20000));
//    Result res3 = pool.submitTask(std::make_shared<MyTask>(20001, 30000));
//    int sum1 = res1.get().cast_<int>();
//    int sum2 = res2.get().cast_<int>();
//    int sum3 = res3.get().cast_<int>();
//    std::cout << (sum1 + sum2 + sum3) << std::endl;

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