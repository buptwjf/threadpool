#include "Threadpool.h"
#include <thread>
#include <iostream>
//#include <utility>

const int TASK_MAX_THRESHOLD = 4;
const int THREAD_MAX_THRESHOLD = 10;

// 线程池的构造函数
Threadpool::Threadpool() :
        initThreadSize_(0),
        taskSize_(0),
        curThreadSize_(0),
        poolMode_(PoolMode::MODE_FIXED),
        taskQueMaxThreshold_(TASK_MAX_THRESHOLD),
        isPoolRunning_(false),
        idleThreadSize(0),
        threadSizeThreashold_(THREAD_MAX_THRESHOLD) {
}

// 线程池的析构函数
Threadpool::~Threadpool() = default;

// 设置线程池的工作模式
void Threadpool::setMode(PoolMode mode) {
    if (checkRunningState()) {
        return;
    }
    poolMode_ = mode;
}

// 开启线程池
void Threadpool::start(int initThreadSize) {
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
        auto ptr = std::make_unique<Thread>([this]() { this->threadFunc(); });
        threads_.emplace_back(std::move(ptr));
    }; // Thread 需要接收一个 函数对象类型
    // 启动所有线程
    for (auto &i: threads_) {
        i->start(); // 执行线程函数
        idleThreadSize++; // 初始化空闲线程的数量
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
        threadSizeThreashold_ = threshold;
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
    if (poolMode_ == PoolMode::MODE_CACHED &&
        taskSize_ > idleThreadSize &&
        curThreadSize_ < threadSizeThreashold_) {
        // 创建新线程 与 start 类似
        auto ptr = std::make_unique<Thread>([this]() { this->threadFunc(); });
        threads_.emplace_back(std::move(ptr));
    }

    // 返回任务对象
    return Result(sp);
}

// 定义线程函数 - 负责消费任务
void Threadpool::threadFunc() {
    auto lastTime = std::chrono::high_resolution_clock().now();
    std::shared_ptr<Task> task;
    for (;;) { // 线程池一直循环
        // 先获取锁
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_);
            std::cout << "tid:" << std::this_thread::get_id()
                      << " try get task... " << std::endl;

            // cached 模式下，多余创建出来的线程如果超过 60s 没有被使用，就应该进行回收
            // 当前时间 - 上一次线程执行的时间 > 60s

            // 等待 notEmpty 条件，
            notEmpty_.wait(lock, [&]() -> bool { return !taskQue_.empty(); });

            idleThreadSize--; // 要去取任务去了

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

        idleThreadSize++; // 线程工作完成了
    }
}

bool Threadpool::checkRunningState() const {
    return isPoolRunning_;
}


////////////////////////// 线程方法实现

// 线程构造
Thread::Thread(Thread::ThreadFunc func) : func_(std::move(func)) {

}

// 线程析构
Thread::~Thread() = default;

// 启动线程
void Thread::start() {
    // 创建一个线程来执行一个线程函数
    std::thread t(func_); // C++ threadpool 线程对象t 和线程函数 func_
    t.detach(); // 设置分离线程211
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

