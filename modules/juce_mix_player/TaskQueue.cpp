#include "TaskQueue.h"

TaskQueue TaskQueue::shared;

void TaskQueue::executeNext() {
    if (stop) {
        return;
    }
    TaskQueue* self = this;
    std::thread thread([&, self]{
        if (self == nullptr || self->stop) {
            return;
        }
        mtx.lock();
        if (self == nullptr || self->stop) {
            return;
        }
        ended = false;
        while (!taskList.empty()) {
            if (self == nullptr || self->stop) {
                return;
            }
            TaskQueueItem& task = taskList.front();
            if (task != nullptr) {
                task();
            }
            if (self == nullptr || self->stop) {
                return;
            }
            taskList.pop_front();
        }
        if (self == nullptr || self->stop) {
            return;
        }
        ended = true;
        mtx.unlock();
    });
    thread.detach();
}

void TaskQueue::async(TaskQueueItem task) {
    if (stop) {
        return;
    }
    taskList.push_back(task);
    if (ended) {
        executeNext();
    }
}

void TaskQueue::stopQueue() {
    taskList.clear();
    stop = true;
}
