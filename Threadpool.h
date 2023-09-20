//
// Created by 86188 on 2023/9/8.
//
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

// 线程池支持的模式
enum class PoolMode {
    MODE_FIXED, // 固定数量的线程池
    MODE_CACHED, // 线程数量可动态增长
};


// Any 类型，可以接受其他任意类型的数据
template <typename T>
class Any {
public:
    Any() = default;

    ~Any() = default;

    // 可以让接收任意类型的数据
    explicit Any(T data) : base_(std::make_unique<Derive>(data)) {}

    //  由于unique_ptr 没有左值的拷贝构造和赋值，因此 Any 类也设置成相同的类型
    Any(const Any &) = delete;
    Any &operator=(const Any &) = delete;

    // 给出右值相关的操作
    Any(Any &&) = default; // 这里不要随便加 const
    Any &operator=(Any &&) = default;

    // Any 能够实现接收任意类型的数据
    // 怎么从 base_ 类找到他所指向 Derive 对象，并去除 data 成员变量
    T cast_() {
        // 基类指针转成 => 派生类指针  RTTI
        Derive *pd = dynamic_cast <Derive *> (base_.get()); // 利用智能指针的获取方法
        if (pd == nullptr) {
            throw std::runtime_error("type is unMatch!");  // expection
        }
        return pd->data_;
    }

private:
    // 基类类型
    class Base {
    public:
        virtual  ~Base() = default;
    };

    // 派生类类型
    class Derive : public Base {
    public:
        explicit Derive(T data) : data_(data) {} // 这里只能写到头文件中
        T data_; // 保证了返回值真正的类型
    };

    // 定义一个基类的指针
    std::unique_ptr<Base> base_;
};

// 任务类型 - 抽象类
class Task {
public:
    virtual void run() = 0;

private:
};

// 线程类型
class Thread {
public:
    // 线程函数对象类型
    using ThreadFunc = std::function<void()>;
    // 线程构造
    explicit Thread(ThreadFunc func);
    // 线程析构
    ~Thread();
    // 启动线程
    void start();

private:
    ThreadFunc func_;

};
/*
 * example:
 * ThreadPool pool
 * pool.start(4);
 *
 * class MyTask : public Task{
 *      public:
 *          void run()  {// 线程代码...}
 *
 * pool.submitTask(std::make_shared<MyTask>());
 * */
// 线程池类型
class Threadpool {
public:
    // 线程池的构造
    Threadpool();

    // 线程池的析构
    ~Threadpool();
    // 设置线程池的工作模式
    void setMode(PoolMode mode);
    // 开启线程池，并设置线程池的初始数量
    void start(int initThreadSize = 4);
    // 设置 task 任务列表上线的阈值
    void setTaskQueMaxThreshold(int threshold);
    // 向线程池上提交任务 用户调用该接口，传入任务对象，生产任务
    void submitTask(const std::shared_ptr<Task> &sp);
    // 禁止拷贝构造和拷贝赋值
    Threadpool(const Threadpool &) = delete;
    Threadpool &operator=(const Threadpool &) = delete;

private:
    // 定义线程函数 - 负责消费任务
    void threadFunc();

private:
    std::vector<std::unique_ptr<Thread>> threads_; // 线程列表 用 智能指针包装起来
    size_t initThreadSize_;          // 初始线程的数量

//    std::queue<Task *> task; // 有可能是临时对象task，因此不能用裸指针，需要延长对象的生命周期
    std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列，使用强智能指针延长临时对象的周期
    std::atomic_int taskSize_; // 任务的数量
    int taskQueMaxThreshold_; // 任务队列数量上限阈值

    // 任务队列必须保证原子操作
    std::mutex taskQueMtx_; // 保证任务队列的线程安全（原子性）
    std::condition_variable notFull_;    // 保证队列不满
    std::condition_variable notEmpty_;   // 保证队列不空
    PoolMode poolMode_; // 当前线程池的工作模式
};

#endif
