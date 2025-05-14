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

#include "../include/gui.h"
#include "../include/app.h"
#include "imgui.h"
#include <windows.h>

// 导航栏实现
void ShowNavigationBar() {
    // 使用BeginMainMenuBar创建主菜单栏
    if (ImGui::BeginMainMenuBar()) {
        // 文件菜单
        if (ImGui::BeginMenu(u8"文件")) {
            ShowFileMenu();
            ImGui::EndMenu();
        }

        // 设置菜单
        if (ImGui::BeginMenu(u8"设置")) {
            ShowSettingsMenu();
            ImGui::EndMenu();
        }

        // 帮助菜单
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
        OPENFILENAMEA ofn;
        char szFile[260] = { 0 };

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "文本文件\0*.txt\0所有文件\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn) == TRUE) {
            g_AppState.currentFilePath = ofn.lpstrFile;
            LoadFile(g_AppState.currentFilePath, g_AppState.fileContent);
        }
    }

    if (ImGui::MenuItem(u8"保存", "Ctrl+S")) {
        if (!g_AppState.currentFilePath.empty()) {
            SaveFile(g_AppState.currentFilePath, g_AppState.fileContent);
        } else {
            OPENFILENAMEA ofn;
            char szFile[260] = { 0 };

            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = GetActiveWindow();
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "文本文件\0*.txt\0所有文件\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
            ofn.lpstrDefExt = "txt";

            if (GetSaveFileNameA(&ofn) == TRUE) {
                g_AppState.currentFilePath = ofn.lpstrFile;
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

    if (ImGui::BeginMenu(u8"高级设置")) {
        if (ImGui::MenuItem(u8"选项 1")) {
            ImGui::OpenPopup(u8"高级选项1");
        }

        if (ImGui::MenuItem(u8"选项 2")) {
            ImGui::OpenPopup(u8"高级选项2");
        }

        // 高级选项1弹窗
        if (ImGui::BeginPopupModal(u8"高级选项1", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(u8"这是高级选项1的设置窗口。");
            if (ImGui::Button(u8"关闭")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // 高级选项2弹窗
        if (ImGui::BeginPopupModal(u8"高级选项2", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(u8"这是高级选项2的设置窗口。");
            if (ImGui::Button(u8"关闭")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndMenu();
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
        ImGui::Text(u8"作者: 示例开发者");
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
        static int tab_index = 0;
        ImGui::BeginTabBar("PreferencesTabBar");

        // 常规设置选项卡
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

        // 编辑器设置选项卡
        if (ImGui::BeginTabItem(u8"编辑器")) {
            ImGui::Text(u8"编辑器设置");
            ImGui::Separator();

            static bool line_numbers = true;
            ImGui::Checkbox(u8"显示行号", &line_numbers);

            static bool syntax_highlight = true;
            ImGui::Checkbox(u8"语法高亮", &syntax_highlight);

            static int tab_size = 4;
            ImGui::SliderInt(u8"Tab 大小", &tab_size, 2, 8);

            static bool word_wrap = false;
            ImGui::Checkbox(u8"自动换行", &word_wrap);

            ImGui::EndTabItem();
        }

        // 主题设置选项卡
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
            // 在这里保存设置
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
        if (ImGui::Button(u8"自定义颜色...")) {
            ImGui::OpenPopup(u8"自定义颜色");
        }

        if (ImGui::BeginPopupModal(u8"自定义颜色", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(u8"自定义界面颜色");
            ImGui::Separator();

            static ImVec4 text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            static ImVec4 window_bg_color = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
            static ImVec4 button_color = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
            
            ImGui::ColorEdit3(u8"文本颜色", (float*)&text_color);
            ImGui::ColorEdit3(u8"窗口背景", (float*)&window_bg_color);
            ImGui::ColorEdit3(u8"按钮颜色", (float*)&button_color);
            
            if (ImGui::Button(u8"应用")) {
                ImGui::GetStyle().Colors[ImGuiCol_Text] = text_color;
                ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = window_bg_color;
                ImGui::GetStyle().Colors[ImGuiCol_Button] = button_color;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"取消")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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
        ImGui::BulletText(u8"'行号'选项可以开启或关闭行号显示");
        ImGui::BulletText(u8"'自动换行'选项可以控制文本换行显示");

        ImGui::Separator();
        ImGui::Text(u8"图表功能说明:");
        ImGui::BulletText(u8"2D图表支持折线图和散点图");
        ImGui::BulletText(u8"3D图表支持多种网格模型展示");
        ImGui::BulletText(u8"可以调整颜色和显示样式");
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
            // 模拟检查更新的进度
            progress += ImGui::GetIO().DeltaTime * 0.5f;
            if (progress > 1.0f) {
                progress = 1.0f;
                checking = false;
                found = (rand() % 2) == 0; // 随机结果，实际应用中应该是真实的检查结果
            }
            
            ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
            ImGui::Text(u8"正在检查更新...");
            
            if (!checking && found) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), 
                    u8"发现新版本 v1.1.0！");
                if (ImGui::Button(u8"立即更新")) {
                    // 这里应该启动更新程序
                    ImGui::OpenPopup(u8"更新确认");
                }
                ImGui::SameLine();
                if (ImGui::Button(u8"稍后更新")) {
                    found = false;
                }
            } else if (!checking && !found) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), 
                    u8"当前已是最新版本！");
            }
        }
        
        // 更新确认弹窗
        if (ImGui::BeginPopupModal(u8"更新确认", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(u8"确认更新到最新版本？\n更新过程中应用将会关闭。");
            ImGui::Separator();
            
            if (ImGui::Button(u8"是", ImVec2(120, 0))) {
                g_AppState.wantToQuit = true; // 退出应用准备更新
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