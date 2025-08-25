#include <iostream>
#include <vector>
#include <thread>
#include <algorithm>
#include <mutex>
#include <condition_variable>

using namespace std;

// 同步变量
mutex mtx;
condition_variable cv;
int current_round = 0;
int ready_threads = 0;

int max_threads = 1;
string str1, str2;
vector<vector<int>> dp;

void initializeDP() {
    int m = str1.length();
    int n = str2.length();
    dp.resize(m + 1, vector<int>(n + 1, 0));
    
    // 初始化边界
    for (int i = 0; i <= m; i++) dp[i][0] = 0;
    for (int j = 0; j <= n; j++) dp[0][j] = 0;
}

void diagonalWorker(int thread_id, int total_rounds) {
    int m = str1.length();
    int n = str2.length();
    
    for (int round = 1; round <= total_rounds; round++) {
        // 同步：等待轮到当前轮次
        {
            unique_lock<mutex> lock(mtx);
            while (current_round < round - 1) {
                cv.wait(lock);
            }
        }
        
        // 计算本线程负责的对角线单元格
        for (int i = 1; i <= min(round, m); i++) {
            int j = round - i + 1;
            if (j < 1 || j > n) continue;
            
            // 负载均衡：根据线程ID分配任务
            if ((i + j) % max_threads == thread_id) {
                if (str1[i - 1] == str2[j - 1]) {
                    dp[i][j] = dp[i - 1][j - 1] + 1;
                } else {
                    dp[i][j] = max(dp[i - 1][j], dp[i][j - 1]);
                }
            }
        }
        
        // 通知本轮完成
        {
            unique_lock<mutex> lock(mtx);
            ready_threads++;
            if (ready_threads == max_threads) {
                current_round = round;
                ready_threads = 0;
                cv.notify_all();
            }
        }
    }
}

int parallelLCS() {
    int m = str1.length();
    int n = str2.length();
    int total_rounds = m + n - 1;
    
    initializeDP();
    
    if (max_threads == 1) {
        // 串行版本
        for (int i = 1; i <= m; i++) {
            for (int j = 1; j <= n; j++) {
                if (str1[i - 1] == str2[j - 1]) {
                    dp[i][j] = dp[i - 1][j - 1] + 1;
                } else {
                    dp[i][j] = max(dp[i - 1][j], dp[i][j - 1]);
                }
            }
        }
    } else {
        // 并行版本 - 重置同步变量
        current_round = 0;
        ready_threads = 0;
        
        vector<thread> threads;
        for (int i = 0; i < max_threads; i++) {
            threads.emplace_back(diagonalWorker, i, total_rounds);
        }
        
        for (auto& t : threads) {
            t.join();
        }
    }
    
    return dp[m][n];
}

int main() {
    cout << "请输入第一个字符串: ";
    cin >> str1;
    cout << "请输入第二个字符串: ";
    cin >> str2;
    cout << "请输入线程数(1-16): ";
    cin >> max_threads;
    
    max_threads = max(1, min(16, max_threads));
    
    int result = parallelLCS();
    cout << "LCS长度: " << result << endl;
    
    return 0;
}
