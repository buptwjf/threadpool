#include "Threadpool.h"
#include <thread>
#include <iostream>
//#include <utility>

const int TASK_MAX_THRESHOLD = 1024;
const int THREAD_MAX_THRESHOLD = 100;
const int THREAD_MAX_IDLE_TIME = 50; // 单位 s
// 线程池的构造函数
Threadpool::Threadpool() :
        initThreadSize_(0),
        taskSize_(0),
        curThreadSize_(0),
        poolMode_(PoolMode::MODE_FIXED),
        taskQueMaxThreshold_(TASK_MAX_THRESHOLD),
        isPoolRunning_(false),
        idleThreadSize_(0),
        threadSizeThreshold_(THREAD_MAX_THRESHOLD) {
}

// 线程池的析构函数
Threadpool::~Threadpool() {
    isPoolRunning_ = false;

    // 等待线程池里面的所有线程返回
    // 两种状态 1. 阻塞  2. 正在执行任务中
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    notEmpty_.notify_all();
    exitCond_.wait(lock, [&]() -> bool { return threads_.size() == 0; });
}

// 设置线程池的工作模式
void Threadpool::setMode(PoolMode mode) {
    if (checkRunningState()) {
        return;
    }
    poolMode_ = mode;
}

// 开启线程池
void Threadpool::start(unsigned int initThreadSize) {
    std::cout << "CPU cores = " << initThreadSize << std::endl;
    // 设置线程池的运行状态
    isPoolRunning_ = true;
    // 记录初始线程个数
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;
    // 创建线程对象 std::vector<Thread *> threads_;

    // 如何向线程注册函数？由线程池定义，线程池需要把线程函数传给线程对象
    // threadFunc 在线程池中定义，那么这个函数就可以访问线程池的成员变量
    for (int i = 0; i < initThreadSize_; ++i) {
//        auto ptr = std::make_unique<Thread>(std::bind(&Threadpool::threadFunc, this));
        // 使用 lambda 表达式实现
//        auto ptr = std::make_unique<Thread>([this](int threadId) { this->threadFunc(threadId); });
        auto ptr = std::make_unique<Thread>(std::bind(&Threadpool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getThreadId();
        threads_.emplace(threadId, std::move(ptr));
    }; // Thread 需要接收一个 函数对象类型
    // 启动所有线程
    for (auto &i: threads_) {
        i.second->start(); // 执行线程函数
        idleThreadSize_++; // 初始化空闲线程的数量
    }
}


// 设置 task 任务列表上线的阈值
void Threadpool::setTaskQueMaxThreshold(int threshold) {
    if (checkRunningState()) {
        return;
    }
    taskQueMaxThreshold_ = threshold;
}

void Threadpool::setThreadSizeThreashold(int threshold) {
    if (checkRunningState()) {
        return;
    }
    if (poolMode_ == PoolMode::MODE_CACHED) {
        threadSizeThreshold_ = threshold;
    }
}

// 向线程池上提交任务
Result Threadpool::submitTask(const std::shared_ptr<Task> &sp) {
    // 作为生产者，首先要获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    // 线程的通信 等待任务队列有空余
    // 这里有个需求，用户不能等待过久？不应该超过1s，负责就要判断任务失败
    if (!notFull_.wait_for(lock, std::chrono::seconds(1),
                           [&]() -> bool { return taskQue_.size() < taskQueMaxThreshold_; })) {
        // 表示 notFull_ 等待 1 s 条件依然没有满足
        std::cerr << "task queue is full, submit task fail." << std::endl;
        return Result(sp, false);
    }

    // 如果有空余，把任务放入任务队列中
    taskQue_.emplace(sp);
    taskSize_++;

    // 因为放入新的任务了，所以需要更新条件变量，在 notEmpty_ 上进行通知
    notEmpty_.notify_one();

    // cached 任务处理模式比较紧急，适合小而快的任务
    // 需要根据任务数量和空闲线程的数量，判断是否需要创建出来新的线程出来？
    if (poolMode_ == PoolMode::MODE_CACHED && // 判断是否需要创建新的线程
        taskSize_ > idleThreadSize_ &&
        curThreadSize_ < threadSizeThreshold_) {
        std::cout << ">>> create new thread !!" << std::endl;
        // 创建新线程 与 start 类似
        auto ptr = std::make_unique<Thread>(std::bind(&Threadpool::threadFunc, this, std::placeholders::_1));
        // 创建线程对象
        int threadId = ptr->getThreadId();
        threads_.emplace(threadId, std::move(ptr)); // 加入新线程
        threads_[threadId]->start(); // 启动新线程
        // 修改线程相关的数量
        curThreadSize_++;
        idleThreadSize_++;
    }

    // 返回任务对象
    return Result(sp);
}

// 定义线程函数 - 负责消费任务
void Threadpool::threadFunc(int threadId) { // 线程函数返回，相应的线程就结束了
    // 线程函数开始的时间
    auto lastTime = std::chrono::high_resolution_clock::now();
    // 实际情况需要所有线程执行完任务以后，线程池才能析构
    for (;;) { // 改用 for
//    while (isPoolRunning_) { // 线程池一直循环
        std::shared_ptr<Task> task;
        // 先获取锁
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_);
            std::cout << "tid:" << std::this_thread::get_id()
                      << " try get task... " << std::endl;

            // cached 模式下，多余创建出来的线程如果超过 60s 没有被使用，就应该进行回收
            // 锁加双重判断
//            while (isPoolRunning_ && taskQue_.size() == 0) {
            while (taskQue_.size() == 0) {
                if (!isPoolRunning_) {
                    threads_.erase(threadId);
                    std::cout << "threadId:" << std::this_thread::get_id() << " exit!" << std::endl;
                    exitCond_.notify_all();
                    return; // 线程函数结束，线程结束
                }
                if (poolMode_ == PoolMode::MODE_CACHED) {
                    if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
                        // 如果是超时
                        auto now = std::chrono::high_resolution_clock::now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        std::cout << "dur = " << dur.count() << std::endl;
                        if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_) {
                            // threadId => thread 对象 然后删除
                            threads_.erase(threadId); // 不要传入 std::this_thread::getid()
                            // 线程数量更改
                            curThreadSize_--;
                            idleThreadSize_--;
                            std::cout << "threadId:" << std::this_thread::get_id() << " exit!" << std::endl;
                            return;
                        }
                    }
                } else {
                    // 等待 notEmpty 条件，
                    notEmpty_.wait(lock);
                }
//                // 线程池要结束了，回收线程资源
//                if (!isPoolRunning_) {
//                    threads_.erase(threadId); // 不要传入 std::this_thread::getid()
//                    std::cout << "threadId:" << std::this_thread::get_id() << " exit!" << std::endl;
//                    exitCond_.notify_all();
//                    return; // 结束线程函数，就是结束当前线程
//                }
            }
            // 线程池要结束了，回收线程资源
//            if (!isPoolRunning_) {
//                break;
//            }

            idleThreadSize_--; // 要去取任务去了

            std::cout << "tid:" << std::this_thread::get_id()
                      << " get task ! " << std::endl;
            // 从任务队列中取一个任务出来
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;
            // 如果依然有剩余任务，继续通知其他线程执行任务
            if (!taskQue_.empty()) {
                notEmpty_.notify_all();
            }
            // 取出一个任务，进行通知
            notFull_.notify_all();
        }// 此时就应该把锁释放掉，否则始终是一个线程在做事

        // 当前线程负责执行这个任务
        if (task != nullptr) {
//            task->run(); // 任务的执行不应该包含在锁里
            task->exec();
        }

        idleThreadSize_++; // 线程工作完成了
        // 更新线程执行完任务的时间
        lastTime = std::chrono::high_resolution_clock::now();
    }


}

bool Threadpool::checkRunningState() const {
    return isPoolRunning_;
}


////////////////////////// 线程方法实现
int Thread::generateId_ = 0;

// 线程构造
Thread::Thread(Thread::ThreadFunc func) : func_(std::move(func)), threadId_(generateId_++) {

}

// 线程析构
Thread::~Thread() = default;

// 启动线程
void Thread::start() {
    // 创建一个线程来执行一个线程函数
    std::thread t(func_, threadId_); // C++ threadpool 线程对象t 和线程函数 func_
    t.detach(); // 设置分离线程211
}

int Thread::getThreadId() const {
    return threadId_;
}
/////////////////////////////////////////////////////////

Any Result::get() {
    if (!isValid_) {
        return "";
    }
    // 如果任务在线程池中还没执行完的话，可以先阻塞用户
    sem_.wait();
    return std::move(any_); // 因为 Any 本来就不能拷贝赋值
}

void Result::setVal(Any any) { // 谁调用的？
    // 存储 task 的返回值
    this->any_ = std::move(any);
    sem_.post(); // 已经获取了任务的返回值，增加信号量资源
}

Result::Result(std::shared_ptr<Task> task, bool isValid)
        : task_(std::move(task)), isValid_(isValid) {
    task_->setResult(this);
}

///////////////////////////////////////////////
void Task::exec() {
    if (result_ != nullptr)
        // 如果 Result 里面也有个 Task 对象的话
        result_->setVal(run()); // 这里发生了多态调用
    // 这个 Result 从哪来，给 Task 增加一个 Result
}

void Task::setResult(Result *res) {
    result_ = res;
}

Task::Task() : result_(nullptr) {}

