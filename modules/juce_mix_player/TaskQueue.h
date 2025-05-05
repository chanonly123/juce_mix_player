#pragma once

#include <deque>
#include <mutex>
#include <functional>
#include <thread>
#include <condition_variable>
#include <atomic>

using TaskQueueItem = std::function<void()>;

class TaskQueue {
public:
    static TaskQueue shared;
    std::string name = "";

    TaskQueue();
    ~TaskQueue();

    void async(TaskQueueItem task);
    void stopQueue();

private:
    void worker();

    std::deque<TaskQueueItem> taskList;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> stop{ false };
    std::thread workerThread;
};
