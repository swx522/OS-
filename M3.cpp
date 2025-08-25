#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <cstring>

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::unordered_map;
using std::istringstream;
using std::fixed;
using std::setprecision;
using std::min;

// 系统调用统计数据结构
struct CallRecord {
    string CallName;
    double DurationSum = 0.0;
    int InvocationCount = 0;
};

void ShowAnalysisResults(const unordered_map<string, CallRecord>& records);

// 提取系统调用名称
string ExtractCallName(const string& line) {
    size_t space_pos = line.find(' ');
    if (space_pos == string::npos) {
        return "INVALID";
    }

    // 检查是否为特殊行（如退出信息）
    if (space_pos + 1 < line.size() && line[space_pos + 1] == '+') {
        return "INVALID";
    }

    size_t paren_pos = line.find('(', space_pos);
    if (paren_pos == string::npos) {
        return "INVALID";
    }

    return line.substr(space_pos + 1, paren_pos - space_pos - 1);
}

// 提取系统调用执行时间
double ExtractCallDuration(const string& line) {
    size_t right_angle = line.rfind('>');
    if (right_angle == string::npos) {
        return 0.0;
    }

    size_t left_angle = line.rfind('<', right_angle);
    if (left_angle == string::npos) {
        return 0.0;
    }

    string time_str = line.substr(left_angle + 1, right_angle - left_angle - 1);
    try {
        return std::stod(time_str);
    }
    catch (...) {
        return 0.0;
    }
}

// 处理strace输出数据
void ProcessStraceData(int read_end, unordered_map<string, CallRecord>& records) {
    const int BUFFER_SIZE = 8192;
    char data_buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    string complete_data;

    while ((bytes_read = read(read_end, data_buffer, BUFFER_SIZE - 1)) > 0) {
        data_buffer[bytes_read] = '\0';
        complete_data.append(data_buffer);
    }

    if (bytes_read < 0) {
        perror("读取数据时发生错误");
        return;
    }

    istringstream data_stream(complete_data);
    string current_line;

    while (getline(data_stream, current_line)) {
        if (current_line.empty()) continue;

        string call_name = ExtractCallName(current_line);
        double call_time = ExtractCallDuration(current_line);

        if (call_name != "INVALID" && call_time > 0.0) {
            records[call_name].CallName = call_name;
            records[call_name].InvocationCount++;
            records[call_name].DurationSum += call_time;
        }
    }

    ShowAnalysisResults(records);
}

// 显示统计分析结果
void ShowAnalysisResults(const unordered_map<string, CallRecord>& records) {
    vector<CallRecord> sorted_records;
    for (const auto& entry : records) {
        sorted_records.push_back(entry.second);
    }

    std::sort(sorted_records.begin(), sorted_records.end(),
        [](const CallRecord& a, const CallRecord& b) {
            return a.DurationSum > b.DurationSum;
        });

    cout << "\n系统调用耗时排名（前5位）：" << endl;
    cout << "===========================================" << endl;
    cout << "调用名称        总耗时(秒)      调用次数" << endl;
    cout << "===========================================" << endl;

    int display_count = min(5, static_cast<int>(sorted_records.size()));
    for (int i = 0; i < display_count; i++) {
        const auto& record = sorted_records[i];

        cout << record.CallName;
        // 格式化对齐
        cout << string(16 - record.CallName.length(), ' ');

        cout << fixed << setprecision(6) << record.DurationSum;
        cout << string(8, ' ');

        cout << record.InvocationCount << endl;
    }
    cout << "===========================================" << endl;
}

// 主执行函数
int main(int arg_count, char* arg_values[]) {
    // 默认测试用例
    if (arg_count < 2) {
        char* default_args[] = { arg_values[0], (char*)"ls", (char*)"-l", nullptr };
        cout << "使用默认测试命令: ls -l" << endl;
        arg_count = 3;
        arg_values = default_args;
    }

    // 准备命令参数
    vector<string> command_args;
    for (int i = 1; i < arg_count; i++) {
        command_args.push_back(arg_values[i]);
    }

    // 创建进程间通信管道
    int communication_pipe[2];
    if (pipe(communication_pipe) == -1) {
        perror("创建管道失败");
        return EXIT_FAILURE;
    }

    pid_t process_id = fork();
    if (process_id == -1) {
        perror("创建子进程失败");
        close(communication_pipe[0]);
        close(communication_pipe[1]);
        return EXIT_FAILURE;
    }

    // 子进程执行strace
    if (process_id == 0) {
        close(communication_pipe[0]);
        dup2(communication_pipe[1], STDERR_FILENO);
        close(communication_pipe[1]);

        // 构建strace参数
        vector<const char*> strace_parameters;
        strace_parameters.push_back("strace");
        strace_parameters.push_back("-T");
        strace_parameters.push_back("-tt");

        for (const auto& arg : command_args) {
            strace_parameters.push_back(arg.c_str());
        }
        strace_parameters.push_back(nullptr);

        execvp("strace", const_cast<char* const*>(strace_parameters.data()));
        perror("执行strace失败");
        _exit(EXIT_FAILURE);
    }

    // 父进程处理数据
    else {
        close(communication_pipe[1]);

        unordered_map<string, CallRecord> analysis_data;
        ProcessStraceData(communication_pipe[0], analysis_data);

        close(communication_pipe[0]);

        int process_status;
        waitpid(process_id, &process_status, 0);
    }

    return EXIT_SUCCESS;
}