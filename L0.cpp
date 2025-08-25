#include <iostream>
#include <cmath>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

#define STBI_NO_GIF
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using std::cout;
using std::endl;
using std::string;
using std::min;

const string DEFAULT_COLOR = "\033[0m";

struct TerminalSize {
    int columns;
    int rows;
};

TerminalSize getTerminalDimensions() {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    return { window.ws_col, window.ws_row };
}

string formatColorValue(int value) {
    string result = std::to_string(value);
    while (result.length() < 3) {
        result = "0" + result;
    }
    return result;
}

void displayImage(const string& imagePath) {
    bool exitRequested = false;

    while (!exitRequested) {
        TerminalSize terminal = getTerminalDimensions();

        int imgWidth, imgHeight, colorChannels;
        unsigned char* pixelData = stbi_load(imagePath.c_str(), &imgWidth, &imgHeight, &colorChannels, 3);
        if (pixelData == nullptr) {
            cout << "图片加载失败: " << imagePath << endl;
            return;
        }

        float scalingFactor = min((float)terminal.columns / imgWidth,
            (float)(terminal.rows - 3) / imgHeight);
        int outputWidth = (int)(imgWidth * scalingFactor);
        int outputHeight = (int)(imgHeight * scalingFactor);

        system("clear");

        cout << "图像尺寸: 原始 " << imgWidth << "x" << imgHeight
            << ", 缩放后 " << outputWidth << "x" << outputHeight << "。" << endl;
        cout << "按下ESC退出，改变窗口大小时自动刷新..." << endl;

        for (int row = 0; row < outputHeight; ++row) {
            for (int col = 0; col < outputWidth; ++col) {
                int sourceX = min((int)(col / scalingFactor), imgWidth - 1);
                int sourceY = min((int)(row / scalingFactor), imgHeight - 1);

                int pixelIndex = (sourceY * imgWidth + sourceX) * 3;
                int red = pixelData[pixelIndex];
                int green = pixelData[pixelIndex + 1];
                int blue = pixelData[pixelIndex + 2];

                string colorCode = "\033[48;2;" + formatColorValue(red) + ";" +
                    formatColorValue(green) + ";" +
                    formatColorValue(blue) + "m";
                cout << colorCode << "  " << DEFAULT_COLOR;
            }
            cout << endl;
        }

        stbi_image_free(pixelData);

        while (true) {
            TerminalSize currentTerminal = getTerminalDimensions();
            if (currentTerminal.columns != terminal.columns ||
                currentTerminal.rows != terminal.rows) {
                break;
            }

            struct termios originalSettings, modifiedSettings;
            tcgetattr(STDIN_FILENO, &originalSettings);
            modifiedSettings = originalSettings;
            modifiedSettings.c_lflag &= ~(ICANON | ECHO);
            modifiedSettings.c_cc[VMIN] = 0;
            modifiedSettings.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &modifiedSettings);

            char inputChar;
            if (read(STDIN_FILENO, &inputChar, 1) > 0) {
                if (inputChar == 27) {
                    exitRequested = true;
                    break;
                }
            }

            tcsetattr(STDIN_FILENO, TCSANOW, &originalSettings);
        }
    }
}

int main(int argCount, char* argValues[]) {
    if (argCount != 2) {
        cout << "使用方法不正确" << endl;
        cout << "正确格式: " << argValues[0] << " <图像文件路径>" << endl;
        return 1;
    }

    string imageFilePath = argValues[1];
    displayImage(imageFilePath);

    return 0;
}