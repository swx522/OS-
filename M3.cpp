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

// ϵͳ����ͳ�����ݽṹ
struct CallRecord {
    string CallName;
    double DurationSum = 0.0;
    int InvocationCount = 0;
};

void ShowAnalysisResults(const unordered_map<string, CallRecord>& records);

// ��ȡϵͳ��������
string ExtractCallName(const string& line) {
    size_t space_pos = line.find(' ');
    if (space_pos == string::npos) {
        return "INVALID";
    }

    // ����Ƿ�Ϊ�����У����˳���Ϣ��
    if (space_pos + 1 < line.size() && line[space_pos + 1] == '+') {
        return "INVALID";
    }

    size_t paren_pos = line.find('(', space_pos);
    if (paren_pos == string::npos) {
        return "INVALID";
    }

    return line.substr(space_pos + 1, paren_pos - space_pos - 1);
}

// ��ȡϵͳ����ִ��ʱ��
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

// ����strace�������
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
        perror("��ȡ����ʱ��������");
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

// ��ʾͳ�Ʒ������
void ShowAnalysisResults(const unordered_map<string, CallRecord>& records) {
    vector<CallRecord> sorted_records;
    for (const auto& entry : records) {
        sorted_records.push_back(entry.second);
    }

    std::sort(sorted_records.begin(), sorted_records.end(),
        [](const CallRecord& a, const CallRecord& b) {
            return a.DurationSum > b.DurationSum;
        });

    cout << "\nϵͳ���ú�ʱ������ǰ5λ����" << endl;
    cout << "===========================================" << endl;
    cout << "��������        �ܺ�ʱ(��)      ���ô���" << endl;
    cout << "===========================================" << endl;

    int display_count = min(5, static_cast<int>(sorted_records.size()));
    for (int i = 0; i < display_count; i++) {
        const auto& record = sorted_records[i];

        cout << record.CallName;
        // ��ʽ������
        cout << string(16 - record.CallName.length(), ' ');

        cout << fixed << setprecision(6) << record.DurationSum;
        cout << string(8, ' ');

        cout << record.InvocationCount << endl;
    }
    cout << "===========================================" << endl;
}

// ��ִ�к���
int main(int arg_count, char* arg_values[]) {
    // Ĭ�ϲ�������
    if (arg_count < 2) {
        char* default_args[] = { arg_values[0], (char*)"ls", (char*)"-l", nullptr };
        cout << "ʹ��Ĭ�ϲ�������: ls -l" << endl;
        arg_count = 3;
        arg_values = default_args;
    }

    // ׼���������
    vector<string> command_args;
    for (int i = 1; i < arg_count; i++) {
        command_args.push_back(arg_values[i]);
    }

    // �������̼�ͨ�Źܵ�
    int communication_pipe[2];
    if (pipe(communication_pipe) == -1) {
        perror("�����ܵ�ʧ��");
        return EXIT_FAILURE;
    }

    pid_t process_id = fork();
    if (process_id == -1) {
        perror("�����ӽ���ʧ��");
        close(communication_pipe[0]);
        close(communication_pipe[1]);
        return EXIT_FAILURE;
    }

    // �ӽ���ִ��strace
    if (process_id == 0) {
        close(communication_pipe[0]);
        dup2(communication_pipe[1], STDERR_FILENO);
        close(communication_pipe[1]);

        // ����strace����
        vector<const char*> strace_parameters;
        strace_parameters.push_back("strace");
        strace_parameters.push_back("-T");
        strace_parameters.push_back("-tt");

        for (const auto& arg : command_args) {
            strace_parameters.push_back(arg.c_str());
        }
        strace_parameters.push_back(nullptr);

        execvp("strace", const_cast<char* const*>(strace_parameters.data()));
        perror("ִ��straceʧ��");
        _exit(EXIT_FAILURE);
    }

    // �����̴�������
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