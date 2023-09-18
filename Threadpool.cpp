#include "Threadpool.h"
#include <thread>
#include <iostream>
//#include <utility>

const int TASK_MAX_THRESHOLD = 4;

// 线程池的构造函数
Threadpool::Threadpool() :
        initThreadSize_(0),
        taskSize_(0),
        poolMode_(PoolMode::MODE_FIXED),
        taskQueMaxThreshold_(TASK_MAX_THRESHOLD) {
}

// 线程池的析构函数
Threadpool::~Threadpool() = default;

// 设置线程池的工作模式
void Threadpool::setMode(PoolMode mode) {
    poolMode_ = mode;
}

// 开启线程池
void Threadpool::start(int initThreadSize) {
    // 记录初始线程个数
    initThreadSize_ = initThreadSize;
    // 创建线程对象 std::vector<Thread *> threads_;

    // 如何向线程注册函数？由线程池定义，线程池需要把线程函数传给线程对象
    // threadFunc 在线程池中定义，那么这个函数就可以访问线程池的成员变量
    for (int i = 0; i < initThreadSize_; ++i) {
//        auto ptr = std::make_unique<Thread>(std::bind(&Threadpool::threadFunc, this));
        // 使用 lambda 表达式实现
        auto ptr = std::make_unique<Thread>([this]() { this->threadFunc(); });
        threads_.emplace_back(std::move(ptr));
    }; // Thread 需要接收一个 函数对象类型
    // 启动所有线程
    for (auto &i: threads_) {
        i->start();
    }
}


// 设置 task 任务列表上线的阈值
void Threadpool::setTaskQueMaxThreshold(int threshold) {
    taskQueMaxThreshold_ = threshold;
}

// 向线程池上提交任务
void Threadpool::submitTask(const std::shared_ptr<Task>& sp) {
    // 作为生产者，首先要获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    // 线程的通信 等待任务队列有空余
    //    while (taskQue_.size() == taskQueMaxThreshold_) {
    //        notFull_.wait(lock); // 1. 线程变为等待状态  2. 并且把锁释放
    //    }

    // 使用更简洁的写法
    /*
     * wait         与时间无关
     * wait_for     等待某一时间长度
     * wait_until   等到某一时刻
     * */
    // 这里有个需求，用户不能等待过久？不应该超过1s，负责就要判断任务失败
    if (!notFull_.wait_for(lock, std::chrono::seconds(1),
                           [&]() -> bool { return taskQue_.size() < taskQueMaxThreshold_; })) {
        // 表示 notFull_ 等待 1 s 条件依然没有满足
        std::cerr << "task queue is full, submit task fail." << std::endl;
    }


    // 如果有空余，把任务放入任务队列中
    taskQue_.emplace(sp);
    taskSize_++;

    // 因为放入新的任务了，所以需要更新条件变量，在 notEmpty_ 上进行通知
    notEmpty_.notify_one();
}

// 定义线程函数 - 负责消费任务
void Threadpool::threadFunc() {
//    std::cout << "begin threadFunc tid:";
//    std::cout << std::this_thread::get_id() << std::endl;
//    std::cout << "end threadFunc tid:";
//    std::cout << std::this_thread::get_id() << std::endl;
    std::shared_ptr<Task> task;
    for (;;) { // 线程池一直循环
        // 先获取锁
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_);
            std::cout << "tid:" << std::this_thread::get_id()
                << " try get task... " << std::endl;
            // 等待 notEmpty 条件
            notEmpty_.wait(lock, [&]() -> bool { return !taskQue_.empty(); });
            std::cout << "tid:" << std::this_thread::get_id()
                      << " get task ! " << std::endl;
            // 从任务队列中取一个任务出来
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;
            // 如果依然有剩余任务，继续通知其他线程执行任务
            if(!taskQue_.empty()){
                notEmpty_.notify_all();
            }
            // 取出一个任务，进行通知
            notFull_.notify_all();
        }// 此时就应该把锁释放掉，否则始终是一个线程在做事

        // 当前线程负责执行这个任务
        if (task != nullptr) {
            task->run(); // 任务的执行不应该包含在锁里
            // 执行完一个任务需要进行通知
        }
    }
}

////////////////////////// 线程方法实现

// 线程构造
Thread::Thread(Thread::ThreadFunc  func) : func_(std::move(func)) {

}

// 线程析构
Thread::~Thread() = default;

// 启动线程
void Thread::start() {
    // 创建一个线程来执行一个线程函数
    std::thread t(func_); // C++ threadpool 线程对象t 和线程函数 func_
    t.detach(); // 设置分离线程211
}
