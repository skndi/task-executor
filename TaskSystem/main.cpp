#include "TaskSystem.hpp"

#include <cassert>
#include <chrono>
#include <optional>
#include <thread>

using namespace TaskSystem;

struct PrinterParams : Task {
  int max;
  int sleep;

  PrinterParams(int max, int sleep) : max(max), sleep(sleep) {}
  virtual std::optional<int> GetIntParam(const std::string &name) const {
    if (name == "max") {
      return max;
    } else if (name == "sleep") {
      return sleep;
    }
    return std::nullopt;
  }
  virtual std::string GetExecutorName() const { return "printer"; }
};

struct RaytracerParams : Task {
  std::string sceneName;

  RaytracerParams(const std::string &sceneName) : sceneName(sceneName) {}
  virtual std::optional<std::string>
  GetStringParam(const std::string &name) const {
    if (name == "sceneName") {
      return sceneName;
    }
    return std::nullopt;
  }
  virtual std::string GetExecutorName() const { return "raytracer"; }
};

void testRenderer() {
  TaskSystemExecutor &ts = TaskSystemExecutor::GetInstance();

  const bool libLoaded = ts.LoadLibrary("./libRaytracerExecutor.so");
  assert(libLoaded);
  std::unique_ptr<Task> task =
      std::make_unique<RaytracerParams>("ManySimpleMeshes");

  TaskSystemExecutor::TaskID id = ts.ScheduleTask(std::move(task), 1);
  ts.WaitForTask(id);
}

void testPrinter() {
  TaskSystemExecutor &ts = TaskSystemExecutor::GetInstance();
#if defined(_WIN32) || defined(_WIN64)
  const bool libLoaded = ts.LoadLibrary("PrinterExecutor.dll");
#elif defined(__APPLE__)
  const bool libLoaded = ts.LoadLibrary("libPrinterExecutor.dylib");
#elif defined(__linux__)
  const bool libLoaded = ts.LoadLibrary("./libPrinterExecutor.so");
#endif
  assert(libLoaded);

  // two instances of the same task
  std::unique_ptr<Task> p1 = std::make_unique<PrinterParams>(100, 25);

  // give some time for the first task to execute
  TaskSystemExecutor::TaskID id1 = ts.ScheduleTask(std::move(p1), 10);
  std::this_thread::sleep_for(std::chrono::milliseconds(30000));

  ts.OnTaskCompleted(
      id1, [](TaskSystemExecutor::TaskID id) { printf("Task 1 finished\n"); });
  ts.WaitForTask(id1);
}

int main(int argc, char *argv[]) {
  TaskSystemExecutor::Init(3);

  testPrinter();

  return 0;
}