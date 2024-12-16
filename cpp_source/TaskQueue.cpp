#include "TaskQueue.h"

TaskQueue* TaskQueueShared = nullptr;

TaskQueue::TaskQueue() : stop_flag(false) {
    worker_thread = std::thread([this]() { this->processTasks(); });
}

TaskQueue::~TaskQueue() {
    stop();
}

void TaskQueue::async(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        tasks.push(task);
    }
    queue_condition.notify_one();
}

void TaskQueue::mainAsync(std::function<void()> task) {

}

void TaskQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop_flag = true;
    }
    queue_condition.notify_all();
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}

void TaskQueue::processTasks() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_condition.wait(lock, [this]() { return stop_flag || !tasks.empty(); });

            if (stop_flag && tasks.empty()) {
                return;
            }

            task = std::move(tasks.front());
            tasks.pop();
        }

        task(); // Execute the task
    }
}

TaskQueue* getTaskQueueShared() {
    if (TaskQueueShared == nullptr) {
        TaskQueueShared = new TaskQueue();
    }
    return TaskQueueShared;
}
