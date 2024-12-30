#include "TaskQueue.h"

TaskQueue TaskQueue::shared;

void TaskQueue::executeNext() {
    std::thread thread([&]{
        mtx.lock();
        ended = false;
        while (!taskList.empty()) {
            TaskQueueItem& task = taskList.front();
            if (task != nullptr) {
                task();
            }
            taskList.pop_front();
        }
        ended = true;
        mtx.unlock();
    });
    thread.detach();
}

void TaskQueue::async(TaskQueueItem task) {
    taskList.push_back(task);
    if (ended) {
        executeNext();
    }
}
