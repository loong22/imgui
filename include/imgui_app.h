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

#ifndef IMGUI_APP_H
#define IMGUI_APP_H

#include <string>
#include <ctime>
#include <fstream>
#include <d3d11.h>
#include <windows.h>

// 程序状态数据结构
struct AppState {
    // 窗口状态
    bool showAboutWindow = false;
    bool showPrefsWindow = false;
    bool showThemeWindow = false;
    bool showHelpWindow = false;
    bool showUpdateWindow = false;
    bool showPlotWindow = false;
    bool show3DPlotWindow = false;
    bool wantToQuit = false;

    // 主题设置
    bool isDarkTheme = true;

    // 文件数据
    std::string currentFilePath;
    std::string fileContent;

    // 日志文件数据
    std::string logFilePath;
    std::string logContent;
    std::streampos lastReadPosition = 0;
    bool autoRefreshLog = true;
    float refreshInterval = 1.0f;
    float lastRefreshTime = 0.0f;
    std::time_t logFileLastModified = 0;
    bool logContentCleared = false;
    bool showLineNumbers = true;
    bool wrapLogLines = true;
    float logLineWidth = 0.0f;
};

extern AppState g_AppState;

// Direct3D 相关全局变量
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern IDXGISwapChain* g_pSwapChain;
extern bool g_SwapChainOccluded;
extern UINT g_ResizeWidth, g_ResizeHeight;
extern ID3D11RenderTargetView* g_mainRenderTargetView;

// === 应用程序核心功能 ===
// 文件操作
bool SaveFile(const std::string& filePath, const std::string& content);
bool LoadFile(const std::string& filePath, std::string& content);
std::time_t GetFileLastModifiedTime(const std::string& filePath);

// 窗口管理
void ConstrainWindowsToMainViewport();

// Direct3D 初始化
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ImPlot相关
void InitializeImPlot();
void CleanupImPlot();

// === GUI界面功能 ===
// 导航栏和菜单
void ShowNavigationBar();
void ShowFileMenu();
void ShowSettingsMenu();
void ShowHelpMenu();

// 各种窗口
void ShowAboutWindow(bool* p_open);
void ShowPreferencesWindow(bool* p_open);
void ShowThemeWindow(bool* p_open);
void ShowHelpWindow(bool* p_open);
void ShowUpdateCheckWindow(bool* p_open);

// === 日志功能 ===
void ShowLogWindow(bool* p_open);
bool LoadLogFileIncremental(const std::string& filePath, std::string& content, std::streampos& lastPos);
void OpenLogFile();
void ClearLogContent();
void RenderLogContent(const std::string& content, bool showLineNumbers, bool wrapText, float wrapWidth);

// === 图表功能 ===
void ShowPlotWindow(bool* p_open);
void DemoMeshPlots(bool* p_open);

#endif // IMGUI_APP_H