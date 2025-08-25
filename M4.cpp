#include <iostream>
#include <cstring>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string>
#include <algorithm>

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::ofstream;
using std::to_string;

// ȫ�ְ�װ����������
long long FunctionWrapperCounter = 0;

// ������ʱ�ļ�·��
string CreateTemporaryFilePath(const string& prefix) {
    string temp_path = prefix;
    temp_path += "XXXXXX";

    vector<char> temp_buffer(temp_path.begin(), temp_path.end());
    temp_buffer.push_back('\0');

    int file_descriptor = mkstemp(temp_buffer.data());
    if (file_descriptor == -1) {
        cout << "������ʱ�ļ�ʧ��: " << strerror(errno) << endl;
        return "ERROR";
    }

    close(file_descriptor);
    return string(temp_buffer.data()) + ".so";
}

// ����Դ����Ϊ�����
bool BuildSharedLibrary(const string& source_code, const string& output_path) {
    string source_file = output_path + ".c";
    ofstream output_stream(source_file);

    if (!output_stream) {
        cout << "�޷�����Դ�ļ�" << endl;
        return false;
    }

    output_stream << source_code;
    output_stream.close();

    pid_t process_id = fork();
    if (process_id < 0) {
        cout << "���̴���ʧ��: " << strerror(errno) << endl;
        return false;
    }

    if (process_id == 0) {
        execlp("gcc", "gcc", "-shared", "-Wno-implicit-function-declaration",
            "-fPIC", "-xc", "-o", output_path.c_str(), source_file.c_str(), nullptr);
        cout << "������ִ��ʧ��: " << strerror(errno) << endl;
        _exit(EXIT_FAILURE);
    }

    int completion_status;
    waitpid(process_id, &completion_status, 0);

    if (WIFEXITED(completion_status) && WEXITSTATUS(completion_status) == 0) {
        remove(source_file.c_str());
        return true;
    }

    remove(source_file.c_str());
    remove(output_path.c_str());
    return false;
}

// �������������
bool ProcessFunctionDefinition(const string& function_code) {
    string library_path = CreateTemporaryFilePath("/tmp/crepl_func_");
    if (library_path == "ERROR") {
        return false;
    }

    if (!BuildSharedLibrary(function_code, library_path)) {
        return false;
    }

    void* library_handle = dlopen(library_path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (!library_handle) {
        cout << "��̬�����ʧ��: " << dlerror() << endl;
        remove(library_path.c_str());
        return false;
    }

    return true;
}

// ������ʽ��ֵ
bool ProcessExpressionEvaluation(const string& expression) {
    FunctionWrapperCounter++;

    string wrapper_name = "__expression_wrapper_" + to_string(FunctionWrapperCounter);
    string wrapper_code = "int " + wrapper_name + "() { return " + expression + "; }";

    string library_path = CreateTemporaryFilePath("/tmp/crepl_expr_");
    if (library_path == "ERROR") {
        return false;
    }

    if (!BuildSharedLibrary(wrapper_code, library_path)) {
        return false;
    }

    void* library_handle = dlopen(library_path.c_str(), RTLD_LAZY);
    if (!library_handle) {
        cout << "��װ�����ʧ��: " << dlerror() << endl;
        remove(library_path.c_str());
        return false;
    }

    typedef int (*ExpressionFunction)();
    ExpressionFunction func_ptr = (ExpressionFunction)dlsym(
        library_handle, wrapper_name.c_str());

    if (!func_ptr) {
        cout << "���Ų���ʧ��: " << dlerror() << endl;
        dlclose(library_handle);
        remove(library_path.c_str());
        return false;
    }

    int computation_result = func_ptr();
    cout << "������: " << computation_result << endl;

    dlclose(library_handle);
    remove(library_path.c_str());
    return true;
}

// ȥ���ַ�����β�հ��ַ�
string TrimWhitespace(const string& input) {
    size_t first_non_ws = input.find_first_not_of(" \t\n\r");
    if (first_non_ws == string::npos) {
        return "";
    }

    size_t last_non_ws = input.find_last_not_of(" \t\n\r");
    return input.substr(first_non_ws, last_non_ws - first_non_ws + 1);
}

// ���������
int main() {
    string user_input;

    while (true) {
        cout << "����ʽ����> ";

        if (!std::getline(std::cin, user_input)) {
            break;
        }

        string processed_input = TrimWhitespace(user_input);
        if (processed_input.empty()) {
            continue;
        }

        // ��⺯�����壨��"int "��ͷ��
        if (processed_input.compare(0, 4, "int ") == 0) {
            if (ProcessFunctionDefinition(processed_input)) {
                cout << "����ɹ�." << endl;
            }
            else {
                cout << "����ʧ��." << endl;
            }
        }
        else {
            if (!ProcessExpressionEvaluation(processed_input)) {
                cout << "��ֵʧ��." << endl;
            }
        }
    }

    return EXIT_SUCCESS;
}