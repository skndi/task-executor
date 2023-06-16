#pragma once

#include "Executor.hpp"
#include "Task.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <ranges>
#include <thread>

namespace TaskSystem {

/**
 * @brief The task system main class that can accept tasks to be scheduled and
 * execute them on multiple threads
 *
 */
struct TaskSystemExecutor {
private:
  TaskSystemExecutor(int threadCount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_threadCount = threadCount;
    m_exit = false;
    for (int32_t i = 0; i < m_threadCount; i++) {
      m_workerThreads.push_back(std::thread([this, i]() { this->run(i); }));
    }
  };

  void run(int32_t threadIndex) {
    std::unique_lock<std::mutex> lock(m_mutex);

    do {
      m_waitForJob.wait(lock, [this]() { return m_tasks.size() || m_exit; });

      if (!m_exit) {
        auto executor = (m_tasks.top()).second;
        lock.unlock();

        Executor::ExecStatus status =
            executor->ExecuteStep(threadIndex, m_threadCount);

        lock.lock();
        if (status == Executor::ExecStatus::ES_Stop) {
          // That's a bug
          m_tasks.pop();
        }
      }
    } while (!m_exit);
  }

public:
  TaskSystemExecutor(const TaskSystemExecutor &) = delete;
  TaskSystemExecutor &operator=(const TaskSystemExecutor &) = delete;

  ~TaskSystemExecutor() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_exit = true;
    m_waitForJob.notify_all();
    lock.unlock();

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

  struct TaskID {};

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
    std::shared_ptr<Executor> exec(
        executors[task->GetExecutorName()](std::move(task)));
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_tasks.push(std::make_pair(priority, exec));
    }
    m_waitForJob.notify_all();
    return TaskID{};
  }

  /**
   * @brief Blocking wait for a given task. Does not block if the task has
   * already finished
   *
   * @param task the task to wait for
   */
  void WaitForTask(TaskID task) { return; }

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
  void Register(const std::string &executorName,
                ExecutorConstructor constructor) {
    executors[executorName] = constructor;
  }

private:
  static TaskSystemExecutor *self;
  std::map<std::string, ExecutorConstructor> executors;
  struct TaskCompare {
    bool operator()(std::pair<int32_t, std::shared_ptr<Executor>> lhs,
                    std::pair<int32_t, std::shared_ptr<Executor>> rhs) {
      return lhs.first < rhs.first;
    }
  };
  std::priority_queue<
      std::pair<int32_t, std::shared_ptr<Executor>>,
      std::vector<std::pair<int32_t, std::shared_ptr<Executor>>>, TaskCompare>
      m_tasks;
  std::vector<std::thread> m_workerThreads;
  int32_t m_threadCount{};
  std::mutex m_mutex;
  std::condition_variable m_waitForJob;
  std::atomic_bool m_exit{true};
};

}; // namespace TaskSystem