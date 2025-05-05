#include "TaskQueue.h"

TaskQueue TaskQueue::shared;

TaskQueue::TaskQueue() {
    workerThread = std::thread([this] { this->worker(); });
}

TaskQueue::~TaskQueue() {
    stopQueue();
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

void TaskQueue::async(TaskQueueItem task) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        taskList.push_back(std::move(task));
    }
    cv.notify_one();
}

void TaskQueue::stopQueue() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        stop = true;
        taskList.clear();
    }
    cv.notify_one(); // wake up worker so it can exit
}

void TaskQueue::worker() {
    while (true) {
        TaskQueueItem task;
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { return stop || !taskList.empty(); });

            if (stop && taskList.empty()) {
                break;
            }

            task = std::move(taskList.front());
            taskList.pop_front();
        }

        if (task) {
            task(); // execute outside lock
        }
    }
}
