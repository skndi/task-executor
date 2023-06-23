#include <cassert>
#include <chrono>
#include <optional>
#include <thread>

#include "TaskSystem.hpp"

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
    virtual std::string GetExecutorName() const {
        return "printer";
    }
};

struct RaytracerParams : Task {
    std::string sceneName;

    RaytracerParams(const std::string &sceneName) : sceneName(sceneName) {}
    virtual std::optional<std::string> GetStringParam(const std::string &name) const {
        if (name == "sceneName") {
            return sceneName;
        }
        return std::nullopt;
    }
    virtual std::string GetExecutorName() const {
        return "raytracer";
    }
};

void testRenderer() {
    TaskSystemExecutor &ts = TaskSystemExecutor::GetInstance();

    const bool libLoaded = ts.LoadLibrary("./libRaytracerExecutor.so");
    assert(libLoaded);
    std::unique_ptr<Task> task = std::make_unique<RaytracerParams>("ManySimpleMeshes");

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

    std::vector<TaskSystemExecutor::TaskID> tasks;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100000; i++) {
        std::unique_ptr<Task> p1 = std::make_unique<PrinterParams>(2, 1);
        tasks.push_back(ts.ScheduleTask(std::move(p1), 1));
    }
    for (const auto &task : tasks) {
        ts.WaitForTask(task);
    }
    auto stop = std::chrono::steady_clock::now();

    printf("Duration: %ld\n", (stop - start).count());
}

int main(int argc, char *argv[]) {
    TaskSystemExecutor::Init(16);

    testPrinter();

    return 0;
}