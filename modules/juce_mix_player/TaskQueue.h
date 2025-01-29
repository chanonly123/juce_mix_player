#pragma once

#include <iostream>
#include <thread>
#include <mutex>
#include <deque>
#include <functional>

typedef std::function<void()> TaskQueueItem;

class TaskQueue {
private:
    std::mutex mtx;
    bool ended = true;
    std::deque<TaskQueueItem> taskList;
    void executeNext();
public:
    std::string name;
    static TaskQueue shared;
    TaskQueue()=default;
    void async(TaskQueueItem task);
};
