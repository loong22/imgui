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

#include "../include/imgui_app.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot3d.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <sys/stat.h>
#include <cmath>

// ===== 应用程序核心功能实现 =====

// 文件操作函数
bool SaveFile(const std::string& filePath, const std::string& content) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    file.close();
    return true;
}

bool LoadFile(const std::string& filePath, std::string& content) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    content.clear();
    std::string line;
    while (std::getline(file, line)) {
        content += line + "\n";
    }
    
    file.close();
    return true;
}

std::time_t GetFileLastModifiedTime(const std::string& filePath) {
    struct stat result;
    if (stat(filePath.c_str(), &result) == 0) {
        return result.st_mtime;
    }
    return 0;
}

// 窗口约束函数
void ConstrainWindowsToMainViewport() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 viewport_min = viewport->Pos;
    ImVec2 viewport_max = ImVec2(viewport->Pos.x + viewport->Size.x,
        viewport->Pos.y + viewport->Size.y);

    ImGuiContext& g = *ImGui::GetCurrentContext();
    for (int i = 0; i < g.Windows.Size; i++) {
        ImGuiWindow* window = g.Windows[i];
        
        if (!window->WasActive || window->Hidden || window->Collapsed || 
            (window->Flags & ImGuiWindowFlags_ChildWindow) ||
            (window->Flags & ImGuiWindowFlags_Tooltip) ||
            (window->Flags & ImGuiWindowFlags_Popup)) {
            continue;
        }

        ImVec2 window_pos = window->Pos;
        ImVec2 window_size = window->Size;
        float min_visible = window->TitleBarHeight > 0.0f ? window->TitleBarHeight : 30.0f;
        bool position_changed = false;

        if (window_pos.x + window_size.x < viewport_min.x + min_visible) {
            window_pos.x = viewport_min.x + min_visible - window_size.x;
            position_changed = true;
        }
        if (window_pos.x > viewport_max.x - min_visible) {
            window_pos.x = viewport_max.x - min_visible;
            position_changed = true;
        }
        if (window_pos.y + window_size.y < viewport_min.y + min_visible) {
            window_pos.y = viewport_min.y + min_visible - window_size.y;
            position_changed = true;
        }
        if (window_pos.y > viewport_max.y - min_visible) {
            window_pos.y = viewport_max.y - min_visible;
            position_changed = true;
        }

        if (position_changed) {
            window->Pos = window_pos;
        }
    }
}

// ImPlot 初始化和清理
void InitializeImPlot() {
    ImPlot::CreateContext();
    ImPlot3D::CreateContext();
}

void CleanupImPlot() {
    ImPlot::DestroyContext();
    ImPlot3D::DestroyContext();
}

// Direct3D 相关函数
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ===== GUI界面功能实现 =====

// 导航栏实现
void ShowNavigationBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu(u8"文件")) {
            ShowFileMenu();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(u8"设置")) {
            ShowSettingsMenu();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(u8"帮助")) {
            ShowHelpMenu();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

// 文件菜单实现
void ShowFileMenu() {
    if (ImGui::MenuItem(u8"新建", "Ctrl+N")) {
        g_AppState.currentFilePath.clear();
        g_AppState.fileContent.clear();
    }

    if (ImGui::MenuItem(u8"打开", "Ctrl+O")) {
        OPENFILENAMEW ofn;
        wchar_t szFile[260] = { 0 };
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
        ofn.lpstrFilter = L"文本文件(*.txt)\0*.txt\0所有文件\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameW(&ofn) == TRUE) {
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, NULL, 0, NULL, NULL);
            std::string path(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, &path[0], size_needed, NULL, NULL);
            if (!path.empty() && path.back() == 0) {
                path.pop_back();
            }
            g_AppState.currentFilePath = path;
            LoadFile(g_AppState.currentFilePath, g_AppState.fileContent);
        }
    }

    if (ImGui::MenuItem(u8"保存", "Ctrl+S")) {
        if (!g_AppState.currentFilePath.empty()) {
            SaveFile(g_AppState.currentFilePath, g_AppState.fileContent);
        } else {
            OPENFILENAMEW ofn;
            wchar_t szFile[260] = { 0 };
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = GetActiveWindow();
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
            ofn.lpstrFilter = L"文本文件(*.txt)\0*.txt\0所有文件\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
            ofn.lpstrDefExt = L"txt";

            if (GetSaveFileNameW(&ofn) == TRUE) {
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, NULL, 0, NULL, NULL);
                std::string path(size_needed, 0);
                WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, &path[0], size_needed, NULL, NULL);
                if (!path.empty() && path.back() == 0) {
                    path.pop_back();
                }
                g_AppState.currentFilePath = path;
                SaveFile(g_AppState.currentFilePath, g_AppState.fileContent);
            }
        }
    }

    ImGui::Separator();
    if (ImGui::MenuItem(u8"退出", "Alt+F4")) {
        g_AppState.wantToQuit = true;
    }
}

// 设置菜单实现
void ShowSettingsMenu() {
    if (ImGui::MenuItem(u8"首选项")) {
        g_AppState.showPrefsWindow = true;
    }
    if (ImGui::MenuItem(u8"颜色主题")) {
        g_AppState.showThemeWindow = true;
    }
}

// 帮助菜单实现
void ShowHelpMenu() {
    if (ImGui::MenuItem(u8"查看文档")) {
        g_AppState.showHelpWindow = true;
    }
    if (ImGui::MenuItem(u8"检查更新")) {
        g_AppState.showUpdateWindow = true;
    }
    if (ImGui::MenuItem(u8"关于")) {
        g_AppState.showAboutWindow = true;
    }
}

// 关于窗口实现
void ShowAboutWindow(bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(u8"关于", p_open, ImGuiWindowFlags_NoResize)) {
        ImGui::Text(u8"Demo GUI 应用程序");
        ImGui::Separator();
        ImGui::Text(u8"版本: 1.0.0");
        ImGui::Text(u8"作者: loong22");
        ImGui::Text(u8"版权所有 © 2025");
        ImGui::Separator();
        ImGui::Text(u8"本程序使用了以下库:");
        ImGui::BulletText("Dear ImGui");
        ImGui::BulletText("DirectX 11");
        ImGui::BulletText("ImPlot");
        ImGui::BulletText("ImPlot3D");
        ImGui::Separator();
        if (ImGui::Button(u8"确定", ImVec2(120, 0))) {
            *p_open = false;
        }
    }
    ImGui::End();
}

// 首选项窗口实现
void ShowPreferencesWindow(bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(u8"首选项", p_open)) {
        ImGui::BeginTabBar("PreferencesTabBar");
        
        if (ImGui::BeginTabItem(u8"常规")) {
            ImGui::Text(u8"常规设置");
            ImGui::Separator();
            static bool auto_save = false;
            ImGui::Checkbox(u8"启用自动保存", &auto_save);
            static int autosave_interval = 5;
            if (auto_save) {
                ImGui::SliderInt(u8"自动保存间隔(分钟)", &autosave_interval, 1, 30);
            }
            static bool show_toolbar = true;
            ImGui::Checkbox(u8"显示工具栏", &show_toolbar);
            static bool confirm_exit = true;
            ImGui::Checkbox(u8"退出前确认", &confirm_exit);
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem(u8"主题")) {
            ImGui::Text(u8"主题设置");
            ImGui::Separator();
            static int theme_index = g_AppState.isDarkTheme ? 0 : 1;
            if (ImGui::RadioButton(u8"深色主题", &theme_index, 0)) {
                g_AppState.isDarkTheme = true;
                ImGui::StyleColorsDark();
            }
            if (ImGui::RadioButton(u8"浅色主题", &theme_index, 1)) {
                g_AppState.isDarkTheme = false;
                ImGui::StyleColorsLight();
            }
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
        ImGui::Separator();
        if (ImGui::Button(u8"保存设置", ImVec2(120, 0))) {
            *p_open = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"取消", ImVec2(120, 0))) {
            *p_open = false;
        }
    }
    ImGui::End();
}

// 主题窗口实现
void ShowThemeWindow(bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(u8"颜色主题", p_open)) {
        static int theme_index = g_AppState.isDarkTheme ? 0 : 1;
        ImGui::Text(u8"选择颜色主题:");
        
        if (ImGui::RadioButton(u8"深色主题", &theme_index, 0)) {
            g_AppState.isDarkTheme = true;
            ImGui::StyleColorsDark();
        }
        if (ImGui::RadioButton(u8"浅色主题", &theme_index, 1)) {
            g_AppState.isDarkTheme = false;
            ImGui::StyleColorsLight();
        }
        
        ImGui::Separator();
        if (ImGui::Button(u8"确定", ImVec2(120, 0))) {
            *p_open = false;
        }
    }
    ImGui::End();
}

// 帮助文档窗口实现
void ShowHelpWindow(bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(u8"帮助文档", p_open)) {
        ImGui::Text(u8"Demo GUI 应用程序使用指南");
        ImGui::Separator();
        ImGui::Text(u8"快捷键:");
        ImGui::BulletText(u8"Ctrl+N: 新建文件");
        ImGui::BulletText(u8"Ctrl+O: 打开文件");
        ImGui::BulletText(u8"Ctrl+S: 保存文件");
        ImGui::BulletText(u8"Alt+F4: 退出程序");
        ImGui::Separator();
        ImGui::Text(u8"日志窗口使用说明:");
        ImGui::BulletText(u8"点击'打开日志文件'选择要监视的日志");
        ImGui::BulletText(u8"'自动刷新'选项可以实时监视日志变化");
        ImGui::BulletText(u8"可以调整刷新间隔来控制更新频率");
        ImGui::Separator();
        ImGui::Text(u8"图表功能说明:");
        ImGui::BulletText(u8"2D图表支持折线图和散点图");
        ImGui::BulletText(u8"3D图表支持多种网格模型展示");
    }
    ImGui::End();
}

// 检查更新窗口实现
void ShowUpdateCheckWindow(bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(u8"检查更新", p_open, ImGuiWindowFlags_NoResize)) {
        static bool checking = false;
        static bool found = false;
        static float progress = 0.0f;
        
        if (!checking) {
            if (ImGui::Button(u8"检查更新")) {
                checking = true;
                found = false;
                progress = 0.0f;
            }
        } else {
            progress += ImGui::GetIO().DeltaTime * 0.5f;
            if (progress > 1.0f) {
                progress = 1.0f;
                checking = false;
                found = (rand() % 2) == 0;
            }
            
            ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
            ImGui::Text(u8"正在检查更新...");
            
            if (!checking && found) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), u8"发现新版本 v1.1.0！");
                if (ImGui::Button(u8"立即更新")) {
                    ImGui::OpenPopup(u8"更新确认");
                }
                ImGui::SameLine();
                if (ImGui::Button(u8"稍后更新")) {
                    found = false;
                }
            } else if (!checking && !found) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), u8"当前已是最新版本！");
            }
        }
        
        if (ImGui::BeginPopupModal(u8"更新确认", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(u8"确认更新到最新版本？\n更新过程中应用将会关闭。");
            ImGui::Separator();
            
            if (ImGui::Button(u8"是", ImVec2(120, 0))) {
                g_AppState.wantToQuit = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"否", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

// ===== 日志功能实现 =====

bool LoadLogFileIncremental(const std::string& filePath, std::string& content, std::streampos& lastPos) {
    if (filePath.empty()) return false;

    struct stat fileInfo;
    if (stat(filePath.c_str(), &fileInfo) != 0) return false;

    std::time_t currentModTime = fileInfo.st_mtime;
    bool fileModified = (currentModTime != g_AppState.logFileLastModified);
    g_AppState.logFileLastModified = currentModTime;

    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize == 0) {
        content.clear();
        lastPos = 0;
        file.close();
        return true;
    }

    bool fileReset = false;
    if (fileSize < lastPos || fileModified || lastPos == 0) {
        lastPos = 0;
        content.clear();
        fileReset = true;
    }

    if (lastPos == fileSize && !fileReset) {
        file.close();
        return true;
    }

    file.seekg(lastPos);
    std::vector<char> buffer(4096);
    std::string newContent;

    while (!file.eof()) {
        file.read(&buffer[0], buffer.size());
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            newContent.append(&buffer[0], bytesRead);
        }
    }

    lastPos = fileSize;
    content += newContent;
    file.close();
    return true;
}

void OpenLogFile() {
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"日志文件*.log*.txt*.dat\0*.log;*.txt;*.dat\0所有文件\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn) == TRUE) {
        std::wstring wFilePath = ofn.lpstrFile;
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wFilePath.c_str(), -1, NULL, 0, NULL, NULL);
        std::string newFilePath(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wFilePath.c_str(), -1, &newFilePath[0], size_needed, NULL, NULL);
        
        if (!newFilePath.empty() && newFilePath.back() == 0) {
            newFilePath.pop_back();
        }

        g_AppState.logFilePath = newFilePath;
        g_AppState.logContent.clear();
        g_AppState.lastReadPosition = 0;
        g_AppState.logFileLastModified = 0;
        g_AppState.logContentCleared = false;
        
        g_AppState.logFileLastModified = GetFileLastModifiedTime(g_AppState.logFilePath);
        LoadLogFileIncremental(g_AppState.logFilePath, g_AppState.logContent, g_AppState.lastReadPosition);
    }
}

void ClearLogContent() {
    g_AppState.logContent.clear();
    g_AppState.logContentCleared = true;
    g_AppState.lastReadPosition = 0;
}

void RenderLogContent(const std::string& content, bool showLineNumbers, bool wrapText, float wrapWidth) {
    if (content.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), u8"暂无日志内容");
        return;
    }

    float effectiveWrapWidth = wrapWidth;
    if (effectiveWrapWidth <= 0.0f) {
        effectiveWrapWidth = ImGui::GetContentRegionAvail().x;
        if (showLineNumbers) effectiveWrapWidth -= 50.0f;
    }

    std::istringstream stream(content);
    std::string line;
    int lineNumber = 1;

    if (showLineNumbers) {
        ImGui::BeginTable("LogContentTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit);
        ImGui::TableSetupColumn("LineNumbers", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);
    }

    while (std::getline(stream, line)) {
        if (showLineNumbers) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%d", lineNumber);
            ImGui::TableSetColumnIndex(1);

            if (wrapText) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + effectiveWrapWidth);
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopTextWrapPos();
            } else {
                ImGui::TextUnformatted(line.c_str());
            }
        } else {
            if (wrapText) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + effectiveWrapWidth);
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopTextWrapPos();
            } else {
                ImGui::TextUnformatted(line.c_str());
            }
        }
        lineNumber++;
    }

    if (showLineNumbers) {
        ImGui::EndTable();
    }
}

void ShowLogWindow(bool* p_open) {
    if (ImGui::Button(u8"打开日志文件")) {
        OpenLogFile();
        g_AppState.logContentCleared = false;
    }

    ImGui::SameLine();
    if (ImGui::Button(u8"清空显示")) {
        ClearLogContent();
    }

    ImGui::SameLine();
    bool prevAutoRefresh = g_AppState.autoRefreshLog;
    ImGui::Checkbox(u8"自动刷新", &g_AppState.autoRefreshLog);

    if (prevAutoRefresh != g_AppState.autoRefreshLog) {
        g_AppState.logContentCleared = false;
        if (g_AppState.autoRefreshLog && !g_AppState.logFilePath.empty()) {
            LoadLogFileIncremental(g_AppState.logFilePath, g_AppState.logContent, g_AppState.lastReadPosition);
            g_AppState.lastRefreshTime = ImGui::GetTime();
        }
    }

    if (g_AppState.autoRefreshLog) {
        ImGui::SameLine();
        ImGui::PushItemWidth(100.0f);
        ImGui::SliderFloat(u8"刷新间隔(秒)", &g_AppState.refreshInterval, 0.1f, 5.0f);
        ImGui::PopItemWidth();

        float currentTime = ImGui::GetTime();
        if (currentTime - g_AppState.lastRefreshTime >= g_AppState.refreshInterval) {
            if (!g_AppState.logFilePath.empty()) {
                if (g_AppState.logContentCleared) {
                    g_AppState.logContent.clear();
                    g_AppState.lastReadPosition = 0;
                    g_AppState.logContentCleared = false;
                }
                LoadLogFileIncremental(g_AppState.logFilePath, g_AppState.logContent, g_AppState.lastReadPosition);
            }
            g_AppState.lastRefreshTime = currentTime;
        }
    } else {
        ImGui::SameLine();
        if (ImGui::Button(u8"刷新")) {
            if (!g_AppState.logFilePath.empty()) {
                if (g_AppState.logContentCleared) {
                    g_AppState.logContent.clear();
                    g_AppState.lastReadPosition = 0;
                    g_AppState.logContentCleared = false;
                }
                LoadLogFileIncremental(g_AppState.logFilePath, g_AppState.logContent, g_AppState.lastReadPosition);
            }
        }
    }

    ImGui::SameLine();
    ImGui::Checkbox(u8"行号", &g_AppState.showLineNumbers);
    ImGui::SameLine();
    ImGui::Checkbox(u8"自动换行", &g_AppState.wrapLogLines);

    if (g_AppState.wrapLogLines) {
        ImGui::SameLine();
        ImGui::PushItemWidth(100.0f);
        ImGui::SliderFloat(u8"行宽", &g_AppState.logLineWidth, 0.0f, 1000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"0 = 自适应窗口宽度");
        ImGui::PopItemWidth();
    }

    ImGui::Separator();

    if (!g_AppState.logFilePath.empty()) {
        struct stat fileInfo;
        bool fileExists = (stat(g_AppState.logFilePath.c_str(), &fileInfo) == 0);

        if (fileExists) {
            std::string sizeStr;
            if (fileInfo.st_size < 1024) {
                sizeStr = std::to_string(fileInfo.st_size) + " B";
            } else if (fileInfo.st_size < 1024 * 1024) {
                sizeStr = std::to_string(fileInfo.st_size / 1024) + " KB";
            } else {
                sizeStr = std::to_string(fileInfo.st_size / (1024 * 1024)) + " MB";
            }

            char timeBuffer[64];
            std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S",
                std::localtime(&fileInfo.st_mtime));

            ImGui::TextWrapped(u8"当前日志文件: %s", g_AppState.logFilePath.c_str());
            ImGui::TextWrapped(u8"文件大小: %s | 最后修改: %s", sizeStr.c_str(), timeBuffer);
            ImGui::TextWrapped(u8"已读取位置: %lld / %lld", (long long)g_AppState.lastReadPosition, (long long)fileInfo.st_size);

            if (g_AppState.logContentCleared) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
                    u8"内容已被清空，下次刷新将重新读取文件");
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 
                u8"文件不存在: %s", g_AppState.logFilePath.c_str());
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), u8"请打开日志文件");
    }

    ImGui::Separator();

    ImGui::BeginChild("LogContent", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    RenderLogContent(g_AppState.logContent, g_AppState.showLineNumbers,
        g_AppState.wrapLogLines, g_AppState.logLineWidth);

    if (!g_AppState.logContentCleared && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
}

// ===== 图表功能实现 =====

void ShowPlotWindow(bool* p_open) {
    if (ImGui::Begin(u8"图表窗口", p_open)) {
        static float xs[100], ys1[100], ys2[100];
        static bool init = true;
        if (init) {
            init = false;
            for (int i = 0; i < 100; ++i) {
                xs[i] = i * 0.01f;
                ys1[i] = sinf(xs[i] * 10.0f);
                ys2[i] = cosf(xs[i] * 10.0f);
            }
        }

        static bool animate = true;
        static float refresh_time = 0;
        if (animate) {
            refresh_time += ImGui::GetIO().DeltaTime;
            for (int i = 0; i < 100; ++i) {
                ys1[i] = sinf(xs[i] * 10.0f + refresh_time);
                ys2[i] = cosf(xs[i] * 10.0f + refresh_time);
            }
        }
        
        ImGui::Checkbox(u8"动态更新", &animate);
        ImGui::SameLine();
        
        static int plot_type = 0;
        ImGui::RadioButton(u8"折线图", &plot_type, 0);
        ImGui::SameLine();
        ImGui::RadioButton(u8"散点图", &plot_type, 1);
        
        // 根据窗口大小动态调整图表区域
        ImVec2 content_region = ImGui::GetContentRegionAvail();
        float plot_area_height = content_region.y - 120.0f; // 预留控件空间
        if (plot_area_height < 200.0f) plot_area_height = 200.0f;
        
        if (ImPlot::BeginPlot(u8"我的图表", ImVec2(-1, plot_area_height))) {
            ImPlot::SetupAxes(u8"X轴", u8"Y轴");
            
            if (plot_type == 0) {
                ImPlot::PlotLine(u8"正弦波", xs, ys1, 100);
                ImPlot::PlotLine(u8"余弦波", xs, ys2, 100);
            } else {
                ImPlot::PlotScatter(u8"正弦波", xs, ys1, 100);
                ImPlot::PlotScatter(u8"余弦波", xs, ys2, 100);
            }
            ImPlot::EndPlot();
        }
        
        ImGui::Separator();
        static bool show_advanced = false;
        if (ImGui::Button(u8"高级设置")) {
            show_advanced = !show_advanced;
        }
        
        if (show_advanced) {
            float settings_height = ImGui::GetContentRegionAvail().y;
            if (settings_height > 120.0f) settings_height = 120.0f;
            
            ImGui::BeginChild("AdvancedSettings", ImVec2(0, settings_height), true);
            
            static float line_thickness = 2.0f;
            ImGui::SliderFloat(u8"线条粗细", &line_thickness, 1.0f, 5.0f);
            ImPlot::GetStyle().LineWeight = line_thickness;
            
            static float marker_size = 3.0f;
            ImGui::SliderFloat(u8"标记大小", &marker_size, 1.0f, 10.0f);
            ImPlot::GetStyle().MarkerSize = marker_size;
            
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void DemoMeshPlots(bool* p_open) {
    if (ImGui::Begin(u8"3D图表窗口", p_open)) {
        static int mesh_id = 0;
        ImGui::Combo("Mesh", &mesh_id, "Duck\0Sphere\0Cube\0\0");

        static bool set_fill_color = true;
        static ImVec4 fill_color = ImVec4(0.8f, 0.8f, 0.2f, 0.6f);
        ImGui::Checkbox("Fill Color", &set_fill_color);
        if (set_fill_color) {
            ImGui::SameLine();
            ImGui::ColorEdit4("##MeshFillColor", (float*)&fill_color);
        }

        static bool set_line_color = true;
        static ImVec4 line_color = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
        ImGui::Checkbox("Line Color", &set_line_color);
        if (set_line_color) {
            ImGui::SameLine();
            ImGui::ColorEdit4("##MeshLineColor", (float*)&line_color);
        }
        
        // 根据窗口大小动态调整3D图表区域
        ImVec2 content_region = ImGui::GetContentRegionAvail();
        float plot3d_area_height = content_region.y - 100.0f; // 预留控件空间
        if (plot3d_area_height < 250.0f) plot3d_area_height = 250.0f;

        if (ImPlot3D::BeginPlot("Mesh Plots", ImVec2(-1, plot3d_area_height))) {
            ImPlot3D::SetupAxesLimits(-1, 1, -1, 1, -1, 1);

            if (set_fill_color)
                ImPlot3D::SetNextFillStyle(fill_color);
            else {
                ImPlot3D::SetNextFillStyle(ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            }
            if (set_line_color)
                ImPlot3D::SetNextLineStyle(line_color);

            if (mesh_id == 0)
                ImPlot3D::PlotMesh("Duck", ImPlot3D::duck_vtx, ImPlot3D::duck_idx, ImPlot3D::DUCK_VTX_COUNT, ImPlot3D::DUCK_IDX_COUNT);
            else if (mesh_id == 1)
                ImPlot3D::PlotMesh("Sphere", ImPlot3D::sphere_vtx, ImPlot3D::sphere_idx, ImPlot3D::SPHERE_VTX_COUNT, ImPlot3D::SPHERE_IDX_COUNT);
            else if (mesh_id == 2)
                ImPlot3D::PlotMesh("Cube", ImPlot3D::cube_vtx, ImPlot3D::cube_idx, ImPlot3D::CUBE_VTX_COUNT, ImPlot3D::CUBE_IDX_COUNT);

            ImPlot3D::EndPlot();
        }
        
        ImGui::Separator();
        static bool auto_rotate = true;
        ImGui::Checkbox(u8"自动旋转", &auto_rotate);
        
        ImGui::Text(u8"操作说明:");
        ImGui::BulletText(u8"左键拖动: 旋转模型");
        ImGui::BulletText(u8"右键拖动: 平移视图");
        ImGui::BulletText(u8"滚轮: 缩放视图");
    }
    ImGui::End();
}