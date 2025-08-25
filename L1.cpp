#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <random>
#include <string>
#include <chrono>
#include <algorithm>
#include <memory>

using namespace std;

// 常量定义
constexpr int MAX_CPU_NUMBER = 8;
constexpr int TASK_OF_EACH_CPU = 5; // 减少任务数量
constexpr long long MAX_BLOCK_SIZE = 16LL * 1024 * 1024;

// 全局变量
int NumberOfCPU = 0;
long long SizeOfStack = 100LL * 1024 * 1024; // 固定100MB内存

// 内存块信息结构体
struct MemoryBlock {
    long long start;
    long long size;

    MemoryBlock(long long s, long long sz) : start(s), size(sz) {}
    
    bool operator==(const MemoryBlock& other) const {
        return start == other.start && size == other.size;
    }
};

// 简化的内存管理器
class MemoryManager {
private:
    vector<MemoryBlock> allocatedBlocks;
    mutable mutex memoryMutex;
    long long totalMemory;
    
public:
    MemoryManager(long long size) : totalMemory(size) {}
    
    unique_ptr<MemoryBlock> allocate(long long size) {
        if (size > MAX_BLOCK_SIZE || size <= 0) {
            return nullptr;
        }
        
        lock_guard<mutex> lock(memoryMutex);
        
        // 简单地从0开始分配，检查是否重叠
        long long start = 0;
        bool found = false;
        
        // 如果没有已分配块，直接分配
        if (allocatedBlocks.empty()) {
            if (size <= totalMemory) {
                found = true;
            }
        } else {
            // 检查第一个块之前
            if (allocatedBlocks[0].start >= size) {
                start = 0;
                found = true;
            } else {
                // 检查块之间
                for (size_t i = 0; i < allocatedBlocks.size() - 1; i++) {
                    long long gap = allocatedBlocks[i + 1].start - (allocatedBlocks[i].start + allocatedBlocks[i].size);
                    if (gap >= size) {
                        start = allocatedBlocks[i].start + allocatedBlocks[i].size;
                        found = true;
                        break;
                    }
                }
                
                // 检查最后一个块之后
                if (!found) {
                    long long lastEnd = allocatedBlocks.back().start + allocatedBlocks.back().size;
                    if (totalMemory - lastEnd >= size) {
                        start = lastEnd;
                        found = true;
                    }
                }
            }
        }
        
        if (found) {
            auto newBlock = make_unique<MemoryBlock>(start, size);
            allocatedBlocks.push_back(*newBlock);
            // 按起始地址排序
            sort(allocatedBlocks.begin(), allocatedBlocks.end(),
                [](const MemoryBlock& a, const MemoryBlock& b) {
                    return a.start < b.start;
                });
            return newBlock;
        }
        
        return nullptr;
    }
    
    void deallocate(unique_ptr<MemoryBlock> block) {
        if (!block) return;
        
        lock_guard<mutex> lock(memoryMutex);
        auto it = find(allocatedBlocks.begin(), allocatedBlocks.end(), *block);
        if (it != allocatedBlocks.end()) {
            allocatedBlocks.erase(it);
        }
    }
    
    size_t getAllocatedBlockCount() const {
        lock_guard<mutex> lock(memoryMutex);
        return allocatedBlocks.size();
    }
};

// 全局内存管理器
MemoryManager memoryManager(SizeOfStack);

// 线程安全的随机数生成器
class ThreadSafeRandom {
private:
    static mt19937& getGenerator() {
        static mutex mtx;
        lock_guard<mutex> lock(mtx);
        static mt19937 generator(random_device{}());
        return generator;
    }
    
public:
    static int getInt(int min, int max) {
        uniform_int_distribution<int> distribution(min, max);
        return distribution(getGenerator());
    }
    
    static long long getLongLong(long long min, long long max) {
        uniform_int_distribution<long long> distribution(min, max);
        return distribution(getGenerator());
    }
};

// 线程安全的输出
class Logger {
private:
    static mutex outputMutex;
    
public:
    static void log(const string& message) {
        lock_guard<mutex> lock(outputMutex);
        cout << message << endl;
    }
};

mutex Logger::outputMutex;

// CPU工作函数
void cpuWork(int cpuNumber) {
    Logger::log("CPU " + to_string(cpuNumber) + " 开始工作。");
    
    for (int i = 0; i < TASK_OF_EACH_CPU; i++) {
        // 简化内存大小选择
        long long memorySize;
        int choice = ThreadSafeRandom::getInt(1, 3);
        
        switch (choice) {
            case 1: memorySize = ThreadSafeRandom::getLongLong(1, 128); break;
            case 2: memorySize = 4 * 1024; break;
            case 3: memorySize = ThreadSafeRandom::getLongLong(4 * 1024, 1 * 1024 * 1024); break;
            default: memorySize = 128;
        }
        
        Logger::log("CPU " + to_string(cpuNumber) + " 尝试申请 " + to_string(memorySize) + " 字节内存。");
        
        auto memoryBlock = memoryManager.allocate(memorySize);
        if (!memoryBlock) {
            Logger::log("CPU " + to_string(cpuNumber) + " 内存申请失败。");
            continue;
        }
        
        Logger::log("CPU " + to_string(cpuNumber) + " 成功申请内存：起始地址=" + 
                   to_string(memoryBlock->start) + "，大小=" + to_string(memoryBlock->size));
        
        // 短暂等待
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // 释放内存
        memoryManager.deallocate(move(memoryBlock));
        Logger::log("CPU " + to_string(cpuNumber) + " 已释放内存块。");
    }
    
    Logger::log("CPU " + to_string(cpuNumber) + " 工作结束。");
}

int main() {
    try {
        // 固定CPU数量
        NumberOfCPU = 4;
        cout << "CPU数目: " << NumberOfCPU << endl;
        cout << "栈堆大小: " << SizeOfStack << " 字节 (" 
             << static_cast<double>(SizeOfStack) / (1024 * 1024) << " MB)" << endl;
        
        // 启动工作线程
        vector<thread> cpuThreads;
        for (int i = 0; i < NumberOfCPU; i++) {
            cpuThreads.emplace_back(cpuWork, i);
        }
        
        // 等待所有线程完成
        for (auto& thread : cpuThreads) {
            thread.join();
        }
        
        cout << "所有CPU工作完成。最终分配块数: " << memoryManager.getAllocatedBlockCount() << endl;
        
    } catch (const exception& e) {
        cerr << "程序异常: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}