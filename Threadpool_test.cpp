// 线程池的头文件

#include "Threadpool.h"
#include "chrono"
#include "thread"
#include "iostream"


class MyTask : public Task {
public:
    void run() override {
        std::cout << std::this_thread::get_id() << " Begin task" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
};


int main() {
    Threadpool pool;
    pool.start(4);

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
    return 0;
}