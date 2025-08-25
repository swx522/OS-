#include "L3.h"

// 系统实现部分

OperatingSystem globalOS;

bool MemoryController::InitializeMemory() {
    int pageCount = GenerateThreadSafeRandom() %
        (MAXIMUM_PAGE_COUNT - MINIMUM_PAGE_COUNT + 1) + MINIMUM_PAGE_COUNT;

    string message = "内存控制器初始化开始。";
    globalOS.DisplayMessage(message);
    cout << "随机页面数量：" << pageCount << " 页，约 "
        << (float)pageCount * MEMORY_PAGE_BYTES / 1048576 << " MB" << endl;

    for (int i = 0; i < pageCount; i++) {
        MemoryPage page;
        page.PageIndex = i;
        MemoryPages.push_back(page);
    }

    message = "内存控制器初始化完成。";
    globalOS.DisplayMessage(message);
    return true;
}

bool MemoryController::ValidateMemoryRange(long long StartAddr, long long Size) {
    long long startPage = StartAddr / MEMORY_PAGE_BYTES;
    long long endPage = (StartAddr + Size - 1) / MEMORY_PAGE_BYTES;

    for (long long i = startPage; i <= endPage; i++) {
        if (MemoryPages[i].AssignedTask != nullptr) {
            return false;
        }
    }
    return true;
}

ProcessTask::ProcessTask(string id) : TaskIdentifier(id) {
    int randomType = GenerateThreadSafeRandom() % 100 + 1;

    if (randomType <= 60) {
        MemoryRequirement = (GenerateThreadSafeRandom() % 128) + 1;
    }
    else if (randomType <= 95) {
        MemoryRequirement = 4LL * 1024;
    }
    else {
        MemoryRequirement = (GenerateThreadSafeRandom() %
            (16LL * 1024 * 1024 - 4 * 1024 + 1)) + 4LL * 1024;
    }

    TotalDuration = (GenerateThreadSafeRandom() % 5) + 1;
    RemainingDuration = TotalDuration;
}

bool ProcessTask::operator<(const ProcessTask& other) const {
    return RemainingDuration > other.RemainingDuration;
}

bool ProcessorUnit::InitializeProcessor() {
    string message = "处理器 " + std::to_string(ProcessorID) + " 初始化开始。";
    globalOS.DisplayMessage(message);

    for (int i = 0; i < TASKS_PER_PROCESSOR; i++) {
        string taskInfo = "[处理器 " + std::to_string(ProcessorID) +
            " 任务 " + std::to_string(i) + " ]";
        ProcessTask task(taskInfo);
        TaskCollection.push_back(task);
    }

    message = "处理器 " + std::to_string(ProcessorID) + " 初始化完成。";
    globalOS.DisplayMessage(message);
    return true;
}

bool OperatingSystem::SystemInitialize() {
    string message = "操作系统初始化开始。";
    DisplayMessage(message);

    SystemMemory.InitializeMemory();

    int cpuCount = GenerateThreadSafeRandom() % MAX_CPU_COUNT + 1;
    message = "随机处理器数量：" + std::to_string(cpuCount);
    DisplayMessage(message);

    for (int i = 0; i < cpuCount; i++) {
        ProcessorUnit cpu;
        cpu.ProcessorID = i;
        cpu.InitializeProcessor();
        Processors.push_back(cpu);
    }

    message = "操作系统初始化完成。";
    DisplayMessage(message);
    return true;
}

void OperatingSystem::StartSystem() {
    vector<thread> processorThreads;
    for (auto& processor : Processors) {
        processorThreads.emplace_back([&processor]() {
            processor.ExecuteTasks();
            });
    }

    for (auto& thread : processorThreads) {
        thread.join();
    }
}

void OperatingSystem::HandleInterrupt(ProcessorUnit& cpu) {
    string msg = "中断：处理器 " + std::to_string(cpu.ProcessorID) +
        "，任务 " + cpu.TaskCollection[cpu.CurrentTaskIndex].TaskIdentifier +
        " （剩余：" + std::to_string(cpu.TaskCollection[cpu.CurrentTaskIndex].RemainingDuration) + " 秒）。";
    DisplayMessage(msg);
}

bool OperatingSystem::AllocateMemory(ProcessorUnit& cpu) {
    lock_guard<mutex> lock(SystemMemory.MemoryAccessMutex);
    long long size = cpu.TaskCollection[cpu.CurrentTaskIndex].MemoryRequirement;

    if (size > (16LL * 1024 * 1024)) {
        return false;
    }

    long long alignment = 1;
    while (alignment < size) {
        alignment *= 2;
    }

    for (long long start = alignment; start <= (long long)MEMORY_PAGE_BYTES *
        (long long)SystemMemory.MemoryPages.size() - size; start += alignment) {

        if (SystemMemory.ValidateMemoryRange(start, size)) {
            long long startPage = start / MEMORY_PAGE_BYTES;
            long long endPage = (start + size - 1) / MEMORY_PAGE_BYTES;

            for (long long i = startPage; i <= endPage; i++) {
                SystemMemory.MemoryPages[i].AssignedTask =
                    &cpu.TaskCollection[cpu.CurrentTaskIndex];
            }

            cpu.TaskCollection[cpu.CurrentTaskIndex].AllocationStart = start;
            return true;
        }
    }
    return false;
}

bool OperatingSystem::ReleaseMemory(ProcessorUnit& cpu) {
    lock_guard<mutex> lock(SystemMemory.MemoryAccessMutex);

    for (auto& page : SystemMemory.MemoryPages) {
        if (page.AssignedTask == &cpu.TaskCollection[cpu.CurrentTaskIndex]) {
            page.AssignedTask = nullptr;
        }
    }

    cpu.TaskCollection[cpu.CurrentTaskIndex].AllocationStart = 0;
    return true;
}

void OperatingSystem::DisplayMessage(string& text) {
    OutputMutex.lock();
    cout << text << endl;
    OutputMutex.unlock();
}

void ProcessorUnit::ExecuteTasks() {
    string message = "处理器 " + std::to_string(ProcessorID) + " 开始执行任务。";
    globalOS.DisplayMessage(message);

    while (!TaskCollection.empty()) {
        auto shortestTask = min_element(TaskCollection.begin(), TaskCollection.end(),
            [](const ProcessTask& a, const ProcessTask& b) {
                return a.RemainingDuration < b.RemainingDuration;
            });

        CurrentTaskIndex = distance(TaskCollection.begin(), shortestTask);

        message = "处理器 " + std::to_string(ProcessorID) +
            " 选择任务：" + TaskCollection[CurrentTaskIndex].TaskIdentifier +
            " （剩余时间：" + std::to_string(TaskCollection[CurrentTaskIndex].RemainingDuration) + " 秒）";
        globalOS.DisplayMessage(message);

        if (TaskCollection[CurrentTaskIndex].AllocationStart == 0) {
            if (!globalOS.AllocateMemory(*this)) {
                message = "处理器 " + std::to_string(ProcessorID) +
                    " 内存分配失败，跳过任务：" + TaskCollection[CurrentTaskIndex].TaskIdentifier;
                globalOS.DisplayMessage(message);
                TaskCollection.erase(TaskCollection.begin() + CurrentTaskIndex);
                continue;
            }
            message = "处理器 " + std::to_string(ProcessorID) +
                " 分配内存起始地址：" + std::to_string(TaskCollection[CurrentTaskIndex].AllocationStart) +
                " (大小:" + std::to_string(TaskCollection[CurrentTaskIndex].MemoryRequirement) + " 字节)";
            globalOS.DisplayMessage(message);
        }

        while (TaskCollection[CurrentTaskIndex].RemainingDuration > 0) {
            sleep_for(seconds(1));
            TaskCollection[CurrentTaskIndex].RemainingDuration--;

            if (GenerateThreadSafeRandom() % 10 < 3) {
                globalOS.HandleInterrupt(*this);

                if (TaskCollection[CurrentTaskIndex].RemainingDuration > 0) {
                    message = "处理器 " + std::to_string(ProcessorID) +
                        " 中断保存: " + TaskCollection[CurrentTaskIndex].TaskIdentifier +
                        " (剩余:" + std::to_string(TaskCollection[CurrentTaskIndex].RemainingDuration) + "秒)";
                    globalOS.DisplayMessage(message);
                }
                break;
            }
        }

        if (TaskCollection[CurrentTaskIndex].RemainingDuration == 0) {
            message = "处理器 " + std::to_string(ProcessorID) +
                " 完成任务: " + TaskCollection[CurrentTaskIndex].TaskIdentifier;
            globalOS.DisplayMessage(message);
            globalOS.ReleaseMemory(*this);
            TaskCollection.erase(TaskCollection.begin() + CurrentTaskIndex);
        }
    }

    message = "处理器 " + std::to_string(ProcessorID) + " 任务执行结束。";
    globalOS.DisplayMessage(message);
}

int GenerateThreadSafeRandom() {
    thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> distribution(0, 2147483647);
    return distribution(generator);
}

int main() {
    globalOS.SystemInitialize();
    globalOS.StartSystem();
    return 0;
}