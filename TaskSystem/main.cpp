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

void test() {
    TaskSystemExecutor &ts = TaskSystemExecutor::GetInstance();
#if defined(_WIN32) || defined(_WIN64)
    bool libLoaded = ts.LoadLibrary("PrinterExecutor.dll");
    libLoaded = ts.LoadLibrary("RaytracerExecutor.dll");
#elif defined(__APPLE__)
    bool libLoaded = ts.LoadLibrary("libPrinterExecutor.dylib");
    libLoaded = ts.LoadLibrary("libRaytracerExecutor.dylib");
#elif defined(__linux__)
    bool libLoaded = ts.LoadLibrary("libPrinterExecutor.so");
    libLoaded = ts.LoadLibrary("libRaytracerExecutor.so");
#endif
    assert(libLoaded);

    std::vector<TaskSystemExecutor::TaskID> tasks;

    std::unique_ptr<Task> p1 = std::make_unique<PrinterParams>(100, 10);
    auto id1 = ts.ScheduleTask(std::move(p1), 1);
    tasks.push_back(id1);
    ts.OnTaskCompleted(id1, [](const TaskSystemExecutor::TaskID &id) { printf("Printer 1 finished\n"); });
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::unique_ptr<Task> task = std::make_unique<RaytracerParams>("HeavyMesh");
    TaskSystemExecutor::TaskID id2 = ts.ScheduleTask(std::move(task), 3);
    ts.OnTaskCompleted(id2, [](const TaskSystemExecutor::TaskID &id) { printf("Render 1 finished\n"); });
    tasks.push_back(id2);

    std::unique_ptr<Task> p2 = std::make_unique<PrinterParams>(100, 10);
    auto id3 = ts.ScheduleTask(std::move(p2), 2);
    ts.OnTaskCompleted(id3, [](const TaskSystemExecutor::TaskID &id) { printf("Printer 2 finished\n"); });
    tasks.push_back(id3);

    std::unique_ptr<Task> task2 = std::make_unique<RaytracerParams>("ManySimpleMeshes");
    TaskSystemExecutor::TaskID id4 = ts.ScheduleTask(std::move(task2), 4);
    ts.OnTaskCompleted(id4, [](const TaskSystemExecutor::TaskID &id) { printf("Render 2 finished\n"); });
    tasks.push_back(id4);

    std::unique_ptr<Task> p3 = std::make_unique<PrinterParams>(100, 100);
    auto id5 = ts.ScheduleTask(std::move(p3), 10);
    ts.OnTaskCompleted(id5, [](const TaskSystemExecutor::TaskID &id) { printf("Printer 3 finished\n"); });
    tasks.push_back(id5);

    for (const auto &task : tasks) {
        ts.WaitForTask(task);
    }

    tasks.clear();

    uint64_t sumTime{};

    int32_t testCount = 10;
    for (int a = 0; a < testCount; a++) {
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 100; i++) {
            std::unique_ptr<Task> p1 = std::make_unique<PrinterParams>(200, 5);
            auto id = ts.ScheduleTask(std::move(p1), rand() % 100);
            tasks.push_back(id);
            ts.OnTaskCompleted(id, [i](const TaskSystemExecutor::TaskID &id) { printf("Task %d finished\n", i); });
            if (i % 20 == 0) {
                std::unique_ptr<Task> task = std::make_unique<RaytracerParams>("HeavyMesh");
                TaskSystemExecutor::TaskID id2 = ts.ScheduleTask(std::move(task), rand() % 100);
                tasks.push_back(id2);
                ts.OnTaskCompleted(id2,
                                   [i](const TaskSystemExecutor::TaskID &id) { printf("Render %d finished\n", i); });
            }
        }

        for (const auto &task : tasks) {
            ts.WaitForTask(task);
        }
        auto stop = std::chrono::steady_clock::now();
        sumTime += (stop - start).count();
    }

    printf("Duration: %ld\n", sumTime / testCount);
}

int main(int argc, char *argv[]) {
    TaskSystemExecutor::Init(std::thread::hardware_concurrency());

    test();

    TaskSystemExecutor::Deinit();

    return 0;
}