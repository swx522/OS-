#ifndef MEMORY_MANAGEMENT_SYSTEM_H
#define MEMORY_MANAGEMENT_SYSTEM_H

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <random>
#include <string>
#include <chrono>
#include <algorithm>

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::thread;
using std::mutex;
using std::lock_guard;
using std::min_element;
using std::this_thread::sleep_for;
using std::chrono::seconds;

// ϵͳ���ó���
constexpr int MAX_PAGE_AMOUNT = 1048576;    // ���ҳ������
constexpr int MIN_PAGE_AMOUNT = 16384;      // ��Сҳ������  
constexpr int MAX_CPU_COUNT = 8;            // CPU�������
constexpr int TASKS_PER_PROCESSOR = 5;      // ÿ������������������
constexpr int PAGE_BYTES = 4096;            // ��ҳ�ֽ���

// ǰ������
struct MemoryPage;
struct MemoryManager;
struct ProcessTask;
struct ProcessorUnit;
struct OperatingSystem;

// �̰߳�ȫ�����������
int GenerateThreadSafeRandom();

// �ڴ�ҳ�ṹ
struct MemoryPage
{
    int PageIndex = 0;                      // ҳ���������
    const int PageCapacity = PAGE_BYTES;    // ҳ��������С
    ProcessTask* AssignedTask = nullptr;    // ռ�ø�ҳ������ָ��
};

// �ڴ������
struct MemoryManager
{
    vector<MemoryPage> MemoryPages;         // �ڴ�ҳ����
    mutex MemoryAccessLock;                 // �ڴ���ʻ�����

    bool InitializeMemory();                // �ڴ��ʼ������
    bool ValidateMemoryRange(long long StartAddr, long long Size); // �ڴ���֤
};

// ��������ṹ
struct ProcessTask
{
    string TaskIdentifier;                  // �����ʶ��
    int ExecutionDuration = 0;              // ��ִ��ʱ��
    int RemainingDuration = 0;              // ʣ��ִ��ʱ��
    long long MemoryRequirement = 0;        // �ڴ������С
    long long AllocationStart = 0;          // �ڴ������ʼ��ַ

    explicit ProcessTask(string id);        // ��ʽ���캯��
    bool operator<(const ProcessTask& other) const; // �Ƚ������
};

// ��������Ԫ
struct ProcessorUnit
{
    int ProcessorID = 0;                    // ��������ʶ
    vector<ProcessTask> TaskCollection;     // ���񼯺�
    int CurrentTaskIndex = 0;               // ��ǰִ����������

    bool InitializeProcessor();             // ��������ʼ��
    void ExecuteTasks();                    // ����ִ�з���
};

// ����ϵͳ
struct OperatingSystem
{
    vector<ProcessorUnit> Processors;       // ����������
    MemoryManager SystemMemory;             // ϵͳ�ڴ����
    mutex OutputLock;                       // ���ͬ����

    bool SystemInitialize();                // ϵͳ��ʼ��
    void StartSystem();                     // ϵͳ��������
    void HandleInterrupt(ProcessorUnit& cpu); // �жϴ���
    bool AllocateMemory(ProcessorUnit& cpu); // �ڴ����
    bool ReleaseMemory(ProcessorUnit& cpu);  // �ڴ��ͷ�
    void DisplayMessage(string& text);      // ��Ϣ��ʾ
};

extern OperatingSystem globalOS;            // ȫ�ֲ���ϵͳʵ��

#endif // MEMORY_MANAGEMENT_SYSTEM_H