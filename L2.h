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

// 系统配置常量
constexpr int MAX_PAGE_AMOUNT = 1048576;    // 最大页面数量
constexpr int MIN_PAGE_AMOUNT = 16384;      // 最小页面数量  
constexpr int MAX_CPU_COUNT = 8;            // CPU最大数量
constexpr int TASKS_PER_PROCESSOR = 5;      // 每个处理器的任务数量
constexpr int PAGE_BYTES = 4096;            // 单页字节数

// 前置声明
struct MemoryPage;
struct MemoryManager;
struct ProcessTask;
struct ProcessorUnit;
struct OperatingSystem;

// 线程安全的随机数生成
int GenerateThreadSafeRandom();

// 内存页结构
struct MemoryPage
{
    int PageIndex = 0;                      // 页面索引编号
    const int PageCapacity = PAGE_BYTES;    // 页面容量大小
    ProcessTask* AssignedTask = nullptr;    // 占用该页的任务指针
};

// 内存管理器
struct MemoryManager
{
    vector<MemoryPage> MemoryPages;         // 内存页集合
    mutex MemoryAccessLock;                 // 内存访问互斥锁

    bool InitializeMemory();                // 内存初始化方法
    bool ValidateMemoryRange(long long StartAddr, long long Size); // 内存验证
};

// 进程任务结构
struct ProcessTask
{
    string TaskIdentifier;                  // 任务标识符
    int ExecutionDuration = 0;              // 总执行时长
    int RemainingDuration = 0;              // 剩余执行时间
    long long MemoryRequirement = 0;        // 内存需求大小
    long long AllocationStart = 0;          // 内存分配起始地址

    explicit ProcessTask(string id);        // 显式构造函数
    bool operator<(const ProcessTask& other) const; // 比较运算符
};

// 处理器单元
struct ProcessorUnit
{
    int ProcessorID = 0;                    // 处理器标识
    vector<ProcessTask> TaskCollection;     // 任务集合
    int CurrentTaskIndex = 0;               // 当前执行任务索引

    bool InitializeProcessor();             // 处理器初始化
    void ExecuteTasks();                    // 任务执行方法
};

// 操作系统
struct OperatingSystem
{
    vector<ProcessorUnit> Processors;       // 处理器集合
    MemoryManager SystemMemory;             // 系统内存管理
    mutex OutputLock;                       // 输出同步锁

    bool SystemInitialize();                // 系统初始化
    void StartSystem();                     // 系统启动运行
    void HandleInterrupt(ProcessorUnit& cpu); // 中断处理
    bool AllocateMemory(ProcessorUnit& cpu); // 内存分配
    bool ReleaseMemory(ProcessorUnit& cpu);  // 内存释放
    void DisplayMessage(string& text);      // 消息显示
};

extern OperatingSystem globalOS;            // 全局操作系统实例

#endif // MEMORY_MANAGEMENT_SYSTEM_H