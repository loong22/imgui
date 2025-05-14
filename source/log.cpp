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

#include "../include/log.h"
#include "../include/app.h"
#include "imgui.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <sys/stat.h>
#include <windows.h>

// 修改LoadLogFileIncremental函数，强化文件读取逻辑
bool LoadLogFileIncremental(const std::string& filePath, std::string& content, std::streampos& lastPos) {
    // 如果文件路径为空，直接返回失败
    if (filePath.empty())
        return false;

    // 检查文件是否存在
    struct stat fileInfo;
    if (stat(filePath.c_str(), &fileInfo) != 0)
        return false;

    // 获取文件的最后修改时间
    std::time_t currentModTime = fileInfo.st_mtime;

    // 检查文件是否被修改（时间戳变化）
    bool fileModified = (currentModTime != g_AppState.logFileLastModified);
    g_AppState.logFileLastModified = currentModTime;

    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open())
        return false;

    // 获取文件大小
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);  // 重置到文件开头

    // 如果文件为空
    if (fileSize == 0) {
        content.clear();
        lastPos = 0;
        file.close();
        return true;
    }

    // 检查文件是否被截断或覆盖，或是否强制读取整个文件
    bool fileReset = false;
    if (fileSize < lastPos || fileModified || lastPos == 0) {
        // 若文件大小减小或修改时间变化，或lastPos是0，从头读取
        lastPos = 0;
        content.clear();
        fileReset = true;
    }

    // 如果没有新内容且没有要求重置，直接返回
    if (lastPos == fileSize && !fileReset) {
        file.close();
        return true;
    }

    // 定位到上次读取的位置
    file.seekg(lastPos);

    // 使用缓冲区读取，避免逐行读取引起的空行问题
    std::vector<char> buffer(4096);
    std::string newContent;

    while (!file.eof()) {
        file.read(&buffer[0], buffer.size());
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            newContent.append(&buffer[0], bytesRead);
        }
    }

    // 更新最后读取位置
    lastPos = fileSize;

    // 添加到已有内容
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
        // 将宽字符路径转换为标准字符串
        std::wstring wFilePath = ofn.lpstrFile;
        
        // 转换为多字节字符串
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wFilePath.c_str(), -1, NULL, 0, NULL, NULL);
        std::string newFilePath(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wFilePath.c_str(), -1, &newFilePath[0], size_needed, NULL, NULL);
        
        // 确保字符串以 null 终止（修剪掉额外的空字符）
        if (!newFilePath.empty() && newFilePath.back() == 0) {
            newFilePath.pop_back();
        }

        // 无论是否选择了相同的文件，都重置状态
        g_AppState.logFilePath = newFilePath;
        g_AppState.logContent.clear();
        g_AppState.lastReadPosition = 0;
        g_AppState.logFileLastModified = 0;
        g_AppState.logContentCleared = false;
        
        // 获取文件修改时间并读取内容
        g_AppState.logFileLastModified = GetFileLastModifiedTime(g_AppState.logFilePath);
        LoadLogFileIncremental(g_AppState.logFilePath, g_AppState.logContent, g_AppState.lastReadPosition);
    }
}

void ClearLogContent() {
    g_AppState.logContent.clear();
    g_AppState.logContentCleared = true; // 设置清空标志
    g_AppState.lastReadPosition = 0;     // 重置读取位置，确保下次刷新时从头读取
}

// 用于解析日志内容，生成带有行号和换行的文本
void RenderLogContent(const std::string& content, bool showLineNumbers, bool wrapText, float wrapWidth) {
    if (content.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), u8"暂无日志内容");
        return;
    }

    // 计算实际的换行宽度，如果是0则使用窗口内容区域宽度
    float effectiveWrapWidth = wrapWidth;
    if (effectiveWrapWidth <= 0.0f) {
        effectiveWrapWidth = ImGui::GetContentRegionAvail().x;
        if (showLineNumbers)
            effectiveWrapWidth -= 50.0f; // 为行号预留空间
    }

    // 解析日志内容
    std::istringstream stream(content);
    std::string line;
    int lineNumber = 1;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const float lineHeight = ImGui::GetTextLineHeight();
    float cursorPosY = ImGui::GetCursorPosY();
    const float startY = cursorPosY;

    // 创建两个列
    if (showLineNumbers) {
        // 使用缩小的列宽来显示行号
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
            // 不显示行号时直接渲染内容
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

// 修改ShowLogWindow函数
void ShowLogWindow(bool* p_open) {
    
    // 文件控制栏
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

    // 如果手动切换自动刷新状态，重置清空标志并立即刷新
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

        // 检查是否应该刷新 - 即使在logContentCleared=true的情况下也尝试刷新
        float currentTime = ImGui::GetTime();
        if (currentTime - g_AppState.lastRefreshTime >= g_AppState.refreshInterval) {
            if (!g_AppState.logFilePath.empty()) {
                // 如果之前清空过内容，那么强制从头读取文件
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
                // 如果之前清空过内容且点击了刷新，那么从头读取
                if (g_AppState.logContentCleared) {
                    g_AppState.logContent.clear();
                    g_AppState.lastReadPosition = 0;
                    g_AppState.logContentCleared = false;
                }
                LoadLogFileIncremental(g_AppState.logFilePath, g_AppState.logContent, g_AppState.lastReadPosition);
            }
        }
    }

    // 添加日志显示选项
    ImGui::SameLine();
    ImGui::Checkbox(u8"行号", &g_AppState.showLineNumbers);

    ImGui::SameLine();
    ImGui::Checkbox(u8"自动换行", &g_AppState.wrapLogLines);

    if (g_AppState.wrapLogLines) {
        ImGui::SameLine();
        ImGui::PushItemWidth(100.0f);
        ImGui::SliderFloat(u8"行宽", &g_AppState.logLineWidth, 0.0f, 1000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(u8"0 = 自适应窗口宽度");
        ImGui::PopItemWidth();
    }

    ImGui::Separator();

    // 显示当前日志文件信息
    if (!g_AppState.logFilePath.empty()) {
        // 获取文件信息
        struct stat fileInfo;
        bool fileExists = (stat(g_AppState.logFilePath.c_str(), &fileInfo) == 0);

        if (fileExists) {
            // 转换文件大小为可读格式
            std::string sizeStr;
            if (fileInfo.st_size < 1024) {
                sizeStr = std::to_string(fileInfo.st_size) + " B";
            } else if (fileInfo.st_size < 1024 * 1024) {
                sizeStr = std::to_string(fileInfo.st_size / 1024) + " KB";
            } else {
                sizeStr = std::to_string(fileInfo.st_size / (1024 * 1024)) + " MB";
            }

            // 转换时间为可读格式
            char timeBuffer[64];
            std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S",
                std::localtime(&fileInfo.st_mtime));

            ImGui::TextWrapped(u8"当前日志文件: %s", g_AppState.logFilePath.c_str());
            ImGui::TextWrapped(u8"文件大小: %s | 最后修改: %s", sizeStr.c_str(), timeBuffer);
            ImGui::TextWrapped(u8"已读取位置: %lld / %lld", (long long)g_AppState.lastReadPosition, (long long)fileInfo.st_size);

            // 添加清空状态显示
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

    // 创建一个子窗口来显示日志内容，支持滚动
    ImGui::BeginChild("LogContent", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    // 使用新的渲染函数显示日志内容
    RenderLogContent(g_AppState.logContent, g_AppState.showLineNumbers,
        g_AppState.wrapLogLines, g_AppState.logLineWidth);

    // 如果内容更新了，自动滚动到底部
    if (!g_AppState.logContentCleared && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
}