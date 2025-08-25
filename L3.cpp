#include "L3.h"

// ϵͳʵ�ֲ���

OperatingSystem globalOS;

bool MemoryController::InitializeMemory() {
    int pageCount = GenerateThreadSafeRandom() %
        (MAXIMUM_PAGE_COUNT - MINIMUM_PAGE_COUNT + 1) + MINIMUM_PAGE_COUNT;

    string message = "�ڴ��������ʼ����ʼ��";
    globalOS.DisplayMessage(message);
    cout << "���ҳ��������" << pageCount << " ҳ��Լ "
        << (float)pageCount * MEMORY_PAGE_BYTES / 1048576 << " MB" << endl;

    for (int i = 0; i < pageCount; i++) {
        MemoryPage page;
        page.PageIndex = i;
        MemoryPages.push_back(page);
    }

    message = "�ڴ��������ʼ����ɡ�";
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
    string message = "������ " + std::to_string(ProcessorID) + " ��ʼ����ʼ��";
    globalOS.DisplayMessage(message);

    for (int i = 0; i < TASKS_PER_PROCESSOR; i++) {
        string taskInfo = "[������ " + std::to_string(ProcessorID) +
            " ���� " + std::to_string(i) + " ]";
        ProcessTask task(taskInfo);
        TaskCollection.push_back(task);
    }

    message = "������ " + std::to_string(ProcessorID) + " ��ʼ����ɡ�";
    globalOS.DisplayMessage(message);
    return true;
}

bool OperatingSystem::SystemInitialize() {
    string message = "����ϵͳ��ʼ����ʼ��";
    DisplayMessage(message);

    SystemMemory.InitializeMemory();

    int cpuCount = GenerateThreadSafeRandom() % MAX_CPU_COUNT + 1;
    message = "���������������" + std::to_string(cpuCount);
    DisplayMessage(message);

    for (int i = 0; i < cpuCount; i++) {
        ProcessorUnit cpu;
        cpu.ProcessorID = i;
        cpu.InitializeProcessor();
        Processors.push_back(cpu);
    }

    message = "����ϵͳ��ʼ����ɡ�";
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
    string msg = "�жϣ������� " + std::to_string(cpu.ProcessorID) +
        "������ " + cpu.TaskCollection[cpu.CurrentTaskIndex].TaskIdentifier +
        " ��ʣ�ࣺ" + std::to_string(cpu.TaskCollection[cpu.CurrentTaskIndex].RemainingDuration) + " �룩��";
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
    string message = "������ " + std::to_string(ProcessorID) + " ��ʼִ������";
    globalOS.DisplayMessage(message);

    while (!TaskCollection.empty()) {
        auto shortestTask = min_element(TaskCollection.begin(), TaskCollection.end(),
            [](const ProcessTask& a, const ProcessTask& b) {
                return a.RemainingDuration < b.RemainingDuration;
            });

        CurrentTaskIndex = distance(TaskCollection.begin(), shortestTask);

        message = "������ " + std::to_string(ProcessorID) +
            " ѡ������" + TaskCollection[CurrentTaskIndex].TaskIdentifier +
            " ��ʣ��ʱ�䣺" + std::to_string(TaskCollection[CurrentTaskIndex].RemainingDuration) + " �룩";
        globalOS.DisplayMessage(message);

        if (TaskCollection[CurrentTaskIndex].AllocationStart == 0) {
            if (!globalOS.AllocateMemory(*this)) {
                message = "������ " + std::to_string(ProcessorID) +
                    " �ڴ����ʧ�ܣ���������" + TaskCollection[CurrentTaskIndex].TaskIdentifier;
                globalOS.DisplayMessage(message);
                TaskCollection.erase(TaskCollection.begin() + CurrentTaskIndex);
                continue;
            }
            message = "������ " + std::to_string(ProcessorID) +
                " �����ڴ���ʼ��ַ��" + std::to_string(TaskCollection[CurrentTaskIndex].AllocationStart) +
                " (��С:" + std::to_string(TaskCollection[CurrentTaskIndex].MemoryRequirement) + " �ֽ�)";
            globalOS.DisplayMessage(message);
        }

        while (TaskCollection[CurrentTaskIndex].RemainingDuration > 0) {
            sleep_for(seconds(1));
            TaskCollection[CurrentTaskIndex].RemainingDuration--;

            if (GenerateThreadSafeRandom() % 10 < 3) {
                globalOS.HandleInterrupt(*this);

                if (TaskCollection[CurrentTaskIndex].RemainingDuration > 0) {
                    message = "������ " + std::to_string(ProcessorID) +
                        " �жϱ���: " + TaskCollection[CurrentTaskIndex].TaskIdentifier +
                        " (ʣ��:" + std::to_string(TaskCollection[CurrentTaskIndex].RemainingDuration) + "��)";
                    globalOS.DisplayMessage(message);
                }
                break;
            }
        }

        if (TaskCollection[CurrentTaskIndex].RemainingDuration == 0) {
            message = "������ " + std::to_string(ProcessorID) +
                " �������: " + TaskCollection[CurrentTaskIndex].TaskIdentifier;
            globalOS.DisplayMessage(message);
            globalOS.ReleaseMemory(*this);
            TaskCollection.erase(TaskCollection.begin() + CurrentTaskIndex);
        }
    }

    message = "������ " + std::to_string(ProcessorID) + " ����ִ�н�����";
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