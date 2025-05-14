/********************************************************************
MIT License

Copyright (c) 2025 loong22

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*********************************************************************/

#ifndef APP_H
#define APP_H

#include <string>
#include <ctime>
#include <d3d11.h>
#include <windows.h>

// 程序状态数据结构
struct AppState {
    bool showAboutWindow = false;      // 是否显示"关于"窗口
    bool showPrefsWindow = false;      // 是否显示"首选项"窗口
    bool showThemeWindow = false;      // 是否显示"颜色主题"窗口
    bool showHelpWindow = false;       // 是否显示"帮助"窗口
    bool showUpdateWindow = false;     // 是否显示"检查更新"窗口
    bool showPlotWindow = false;       // 是否显示"图表"窗口
    bool show3DPlotWindow = false;     // 是否显示"3D图表"窗口

    // 主题设置
    bool isDarkTheme = true;

    // 文件数据
    std::string currentFilePath;
    std::string fileContent;

    // 退出应用程序
    bool wantToQuit = false;

    // 日志文件数据
    std::string logFilePath;           // 日志文件路径
    std::string logContent;            // 缓存的日志内容
    std::streampos lastReadPosition;   // 上次读取位置
    bool autoRefreshLog = true;        // 是否自动刷新日志
    float refreshInterval = 1.0f;      // 刷新间隔(秒)
    float lastRefreshTime = 0.0f;      // 上次刷新时间
    std::time_t logFileLastModified = 0; // 日志文件的最后修改时间
    bool logContentCleared = false;    // 日志内容是否被清空
    bool showLineNumbers = true;       // 是否显示行号
    bool wrapLogLines = true;          // 是否自动换行
    float logLineWidth = 0.0f;         // 日志行宽（0表示自适应窗口宽度）
};

extern AppState g_AppState;

// Direct3D 相关全局变量
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern IDXGISwapChain* g_pSwapChain;
extern bool g_SwapChainOccluded;
extern UINT g_ResizeWidth, g_ResizeHeight;
extern ID3D11RenderTargetView* g_mainRenderTargetView;

// 文件操作函数
bool SaveFile(const std::string& filePath, const std::string& content);
bool LoadFile(const std::string& filePath, std::string& content);

// 窗口约束函数
void ConstrainWindowsToMainViewport();

// Direct3D 初始化函数
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ImPlot相关函数
void InitializeImPlot();
void CleanupImPlot();

// 获取文件的最后修改时间
std::time_t GetFileLastModifiedTime(const std::string& filePath);

#endif // APP_H