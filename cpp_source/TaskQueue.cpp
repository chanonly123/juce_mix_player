#pragma once

#include <iostream>
#include <queue>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

class TaskQueue {
public:
    TaskQueue() : stop_flag(false) {
        worker_thread = std::thread([this]() { this->processTasks(); });
    }

    ~TaskQueue() {
        stop();
    }

    // Asynchronous task execution
    void async(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            tasks.push(task);
        }
        queue_condition.notify_one();
    }

    // Synchronous task execution
    void sync(std::function<void()> task) {
        task(); // Execute task immediately
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            stop_flag = true;
        }
        queue_condition.notify_all();
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }

private:
    void processTasks() {
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

    std::queue<std::function<void()>> tasks;
    std::thread worker_thread;
    std::mutex queue_mutex;
    std::condition_variable queue_condition;
    std::atomic<bool> stop_flag;
};

