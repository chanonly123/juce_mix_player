#pragma once

#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class TaskQueue {
public:

    TaskQueue();
    ~TaskQueue();

    void async(std::function<void()> task);

    void stop();

private:
    void processTasks();

    std::queue<std::function<void()>> tasks;
    std::thread worker_thread;
    std::mutex queue_mutex;
    std::condition_variable queue_condition;
    std::atomic<bool> stop_flag;
};

TaskQueue* getTaskQueueShared();
