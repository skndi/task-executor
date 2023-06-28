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
    std::unique_ptr<Task> task = std::make_unique<RaytracerParams>("ManyHeavyMeshes");

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
    bool libLoaded = ts.LoadLibrary("./libPrinterExecutor.so");
    libLoaded = ts.LoadLibrary("./libRaytracerExecutor.so");
#endif
    assert(libLoaded);

    std::vector<TaskSystemExecutor::TaskID> tasks;
    uint64_t sumTime{};

    int32_t testCount = 100;
    for (int a = 0; a < testCount; a++) {
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000; i++) {
            std::unique_ptr<Task> p1 = std::make_unique<PrinterParams>(100, 1);
            auto id = ts.ScheduleTask(std::move(p1), rand() % 100);
            tasks.push_back(id);
            if (i % 2 == 0) {
                ts.OnTaskCompleted(id, [i](TaskSystemExecutor::TaskID id) { /* printf("Task %d finished\n", i); */ });
            }
        }

        for (const auto &task : tasks) {
            ts.WaitForTask(task);
        }
        auto stop = std::chrono::steady_clock::now();
        sumTime += (stop - start).count();
    }

    printf("Duration: %ld\n", sumTime / testCount);

    /* std::unique_ptr<Task> p1 = std::make_unique<PrinterParams>(10000, 10);
    auto id1 = ts.ScheduleTask(std::move(p1), 1);
    tasks.push_back(id1);
    ts.OnTaskCompleted(id1, [](TaskSystemExecutor::TaskID id) { printf("Task 1 finished\n"); });
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::unique_ptr<Task> task = std::make_unique<RaytracerParams>("HeavyMesh");
    TaskSystemExecutor::TaskID id2 = ts.ScheduleTask(std::move(task), 3);
    ts.OnTaskCompleted(id2, [](TaskSystemExecutor::TaskID id2) { printf("Render finished\n"); });
    tasks.push_back(id2);

    std::unique_ptr<Task> p2 = std::make_unique<PrinterParams>(100, 100);
    auto id3 = ts.ScheduleTask(std::move(p2), 2);
    ts.OnTaskCompleted(id3, [](TaskSystemExecutor::TaskID id) { printf("Task 2 finished\n"); });
    tasks.push_back(id3);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::unique_ptr<Task> task2 = std::make_unique<RaytracerParams>("ManyHeavyMeshes");
    TaskSystemExecutor::TaskID id4 = ts.ScheduleTask(std::move(task2), 4);
    ts.OnTaskCompleted(id4, [](TaskSystemExecutor::TaskID id4) { printf("Render finished\n"); });
    tasks.push_back(id4);

    for (const auto &task : tasks) {
        ts.WaitForTask(task);
    } */
}

int main(int argc, char *argv[]) {
    TaskSystemExecutor::Init(64);

    testPrinter();
    return 0;
}