#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <ranges>
#include <thread>
#include <unordered_set>

#include "Executor.hpp"
#include "Task.hpp"
namespace TaskSystem {

/**
 * @brief The task system main class that can accept tasks to be scheduled and
 * execute them on multiple threads
 *
 */

struct TaskSystemExecutor {
    using ExecutorPtr = std::shared_ptr<Executor>;
    using TaskID = std::pair<uint64_t, int32_t>;
    using TaskCompletionCallback = std::function<void(TaskID)>;

private:
    TaskSystemExecutor(int threadCount) : m_threadCount{threadCount}, m_recheckTask(m_threadCount) {
        std::lock_guard<std::mutex> lock(m_jobMutex);

        for (int32_t i = 0; i < m_threadCount; i++) {
            m_workerThreads.push_back(std::thread([this, i]() { this->runJob(i); }));
        }
    };

    void runJob(int32_t threadIndex) {
        std::unique_lock<std::mutex> lock(m_jobMutex);
        auto &recheckTask = m_recheckTask[threadIndex];

        do {
            m_waitForJob.wait(lock, [this]() { return m_tasks.size() || m_exit.load(); });

            if (!m_exit.load()) {
                auto task = m_tasks.begin();
                ExecutorPtr executor = task->second;
                TaskID key = task->first;
                recheckTask.store(false, std::memory_order_relaxed);
                lock.unlock();

                Executor::ExecStatus status{Executor::ExecStatus::ES_Continue};
                // This inner loop is to skip rechecking the map on every iteration
                while (status != Executor::ExecStatus::ES_Stop && !recheckTask.load(std::memory_order_relaxed)) {
                    status = executor->ExecuteStep(threadIndex, m_threadCount);
                }

                lock.lock();
                if (status == Executor::ExecStatus::ES_Stop && m_uncompletedTasksCallbackList.contains(key.first)) {
                    m_tasks.erase(key);
                    TaskCompletionCallback cb = std::move(m_uncompletedTasksCallbackList[key.first]);
                    m_uncompletedTasksCallbackList.erase(key.first);
                    lock.unlock();
                    m_waitForCompletion.notify_all();
                    if (cb) {
                        cb(key);
                    }
                    lock.lock();
                }
            }
        } while (!m_exit.load());
    }

public:
    TaskSystemExecutor(const TaskSystemExecutor &) = delete;
    TaskSystemExecutor &operator=(const TaskSystemExecutor &) = delete;

    ~TaskSystemExecutor() {
        std::unique_lock<std::mutex> lock(m_jobMutex);
        m_exit = true;
        // We reuse recheck task here, to save a comparison with m_exit in the inner while loop
        for (auto &recheckFlag : m_recheckTask) {
            recheckFlag.store(true, std::memory_order_relaxed);
        }
        lock.unlock();
        m_waitForJob.notify_all();

        for (auto &thread : m_workerThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    static TaskSystemExecutor &GetInstance();

    /**
     * @brief Initialisation called once by the main application to allocate
     * needed resources and start threads
     *
     * @param threadCount the desired number of threads to utilize
     */
    static void Init(int threadCount) {
        delete self;
        self = new TaskSystemExecutor(threadCount);
    }

    static void Deinit() {
        delete self;
        self = nullptr;
    }

    /**
     * @brief Schedule a task with specific priority to be executed
     *
     * @param task the parameters describing the task, Executor will be
     * instantiated based on the expected name
     * @param priority the task priority, bigger means executer sooner
     * @return TaskID unique identifier used in later calls to wait or schedule
     * callbacks for tasks
     */
    TaskID ScheduleTask(std::unique_ptr<Task> task, int priority) {
        uint64_t taskID{};
        {
            std::lock_guard<std::mutex> taskLock(m_jobMutex);
            taskID = m_currentTaskID;
            m_currentTaskID++;

            m_uncompletedTasksCallbackList.emplace(taskID, nullptr);
            m_tasks.emplace(std::make_pair(taskID, priority), executors[task->GetExecutorName()](std::move(task)));
        }
        for (auto &recheckFlag : m_recheckTask) {
            recheckFlag.store(true, std::memory_order_relaxed);
        }
        m_waitForJob.notify_all();
        return TaskID{taskID, priority};
    }

    /**
     * @brief Blocking wait for a given task. Does not block if the task has
     * already finished
     *
     * @param task the task to wait for
     */
    void WaitForTask(TaskID task) {
        std::unique_lock<std::mutex> lock(m_jobMutex);
        m_waitForCompletion.wait(
            lock, [this, &task]() -> bool { return !m_uncompletedTasksCallbackList.contains(task.first); });
        return;
    }

    /**
     * @brief Register a callback to be executed when a task has finished
     * executing. Executes the callbacl immediately if the task has already
     * finished
     *
     * @param task the task that was previously scheduled
     * @param callback the callback to be executed
     */
    void OnTaskCompleted(TaskID task, TaskCompletionCallback &&callback) {
        std::unique_lock<std::mutex> lock(m_jobMutex);
        if (!m_uncompletedTasksCallbackList.contains(task.first)) {
            lock.unlock();
            callback(task);
            return;
        }

        m_uncompletedTasksCallbackList[task.first] = std::move(callback);
    }

    /**
     * @brief Load a dynamic library from a path and attempt to call OnLibraryInit
     *
     * @param path the path to the dynamic library
     * @return true when OnLibraryInit is found, false otherwise
     */
    bool LoadLibrary(const std::string &path);

    /**
     * @brief Register an executor with a name and constructor function. Should be
     * called from inside the dynamic libraries defining executors.
     *
     * @param executorName the name associated with the executor
     * @param constructor constructor returning new instance of the executor
     */
    void Register(const std::string &executorName, ExecutorConstructor constructor) {
        std::lock_guard<std::mutex> lock(m_jobMutex);
        executors[executorName] = constructor;
    }

private:
    static TaskSystemExecutor *self;
    std::unordered_map<std::string, ExecutorConstructor> executors;
    struct TaskEntryComparator {
        bool operator()(const TaskID &lhs, const TaskID &rhs) const {
            // We sort by priority and use the taskID as a tie-breaker. Latency is lowered by keeping the order of entry
            // between tasks with the same priority
            return (lhs.second > rhs.second) || (!(lhs.second < rhs.second) && (lhs.first < rhs.first));
        }
    };
    // This will eventually overflow, but it's such a big number it doesn't really matter
    uint64_t m_currentTaskID{};
    int32_t m_threadCount{};
    std::atomic_bool m_exit{false};
    // Try std::priority_queue + std::unordered_map for checking items
    std::map<TaskID, ExecutorPtr, TaskEntryComparator> m_tasks;
    std::unordered_map<uint64_t, TaskCompletionCallback> m_uncompletedTasksCallbackList;
    std::vector<std::thread> m_workerThreads;
    std::vector<std::atomic_bool> m_recheckTask;

    std::mutex m_jobMutex;
    std::condition_variable m_waitForJob;
    std::condition_variable m_waitForCompletion;
};
};  // namespace TaskSystem