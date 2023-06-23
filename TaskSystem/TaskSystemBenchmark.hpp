#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <ranges>
#include <thread>
#include <unordered_map>

#include "Executor.hpp"
#include "ReadWriteLock.hpp"
#include "Task.hpp"
namespace TaskSystem {

/**
 * @brief The task system main class that can accept tasks to be scheduled and
 * execute them on multiple threads
 *
 */

using ExecutorPtr = std::shared_ptr<Executor>;
using TaskKey = std::pair<int32_t, uint64_t>;

struct TaskSystemExecutor {
private:
    TaskSystemExecutor(int threadCount) : m_recheckTask(threadCount) {
        std::lock_guard<std::mutex> lock(m_jobMutex);
        m_threadCount = threadCount;
        m_exit = false;

        for (int32_t i = 0; i < m_threadCount; i++) {
            m_recheckTask[i].store(false);
            m_workerThreads.push_back(std::thread([this, i]() { this->runJob(i); }));
        }
    };

    void runJob(int32_t threadIndex) {
        std::unique_lock<std::mutex> lock(m_jobMutex);

        do {
            // Use read/write locks
            m_waitForJob.wait(lock, [this]() { return m_tasks.size() || m_exit; });

            if (!m_exit) {
                auto topTask = m_tasks.begin();
                ExecutorPtr executor = topTask->second;
                TaskKey key = topTask->first;
                m_recheckTask[threadIndex].store(false);
                lock.unlock();

                Executor::ExecStatus status{Executor::ExecStatus::ES_Continue};
                while (status != Executor::ExecStatus::ES_Stop && !m_recheckTask[threadIndex].load()) {
                    status = executor->ExecuteStep(threadIndex, m_threadCount);
                }

                if (status == Executor::ExecStatus::ES_Stop) {
                    std::lock_guard<std::mutex> writeLock(m_jobMutex);
                    if (m_tasks.find(key) != m_tasks.end()) {
                        m_tasks.erase(key);
                        m_waitForCompletion.notify_all();
                    }
                }
                lock.lock();
            }
        } while (!m_exit);
    }

public:
    TaskSystemExecutor(const TaskSystemExecutor &) = delete;
    TaskSystemExecutor &operator=(const TaskSystemExecutor &) = delete;

    ~TaskSystemExecutor() {
        std::unique_lock<std::mutex> lock(m_jobMutex);
        m_exit = true;
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

    struct TaskID {
        uint64_t id;
        int32_t priority;
    };

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
        {
            std::unique_lock<std::mutex> lock(m_jobMutex);
            TaskKey key = std::make_pair(priority, m_currentTaskId);
            m_tasks.emplace(key, executors[task->GetExecutorName()](std::move(task)));
        }

        for (auto &flag : m_recheckTask) {
            flag.store(true);
        }
        m_waitForJob.notify_all();
        return TaskID{m_currentTaskId++, priority};
    }

    /**
     * @brief Blocking wait for a given task. Does not block if the task has
     * already finished
     *
     * @param task the task to wait for
     */
    void WaitForTask(TaskID task) {
        std::unique_lock<std::mutex> lock(m_jobMutex);
        auto key = std::make_pair(task.priority, task.id);
        m_waitForCompletion.wait(lock, [this, &key]() -> bool { return m_tasks.find(key) == m_tasks.end(); });
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
    void OnTaskCompleted(TaskID task, std::function<void(TaskID)> &&callback) {
        callback(task);
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
        executors[executorName] = constructor;
    }

private:
    static TaskSystemExecutor *self;
    std::unordered_map<std::string, ExecutorConstructor> executors;
    struct TaskEntryComparator {
        bool operator()(const TaskKey &lhs, const TaskKey &rhs) const {
            if (lhs.first > rhs.first) {
                return true;
            } else if (lhs.first < rhs.first) {
                return false;
            }  // The compare by ID breaks when the uint64_t wraps, but since
            // that's hardly feasible we should be fine
            else if (lhs.second < rhs.second) {
                return true;
            } else {
                return false;
            }
        }
    };
    std::map<TaskKey, ExecutorPtr, TaskEntryComparator> m_tasks;
    std::vector<std::thread> m_workerThreads;
    std::vector<std::thread> m_notificationThreads;
    std::vector<std::atomic_bool> m_recheckTask;
    int32_t m_threadCount{};
    uint64_t m_currentTaskId{};
    std::mutex m_jobMutex;
    std::condition_variable m_waitForJob;
    std::condition_variable m_waitForCompletion;
    std::atomic_bool m_exit{true};
};

};  // namespace TaskSystem