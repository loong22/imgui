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

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "implot.h"
#include "implot3d.h"
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <fstream>
#include <sstream>
#include <ctime>
#include <vector>

// 引入头文件
#include "../include/app.h"
#include "../include/gui.h"
#include "../include/log.h"
#include "../include/plot.h"

// 初始化全局变量
AppState g_AppState;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
bool g_SwapChainOccluded = false;
UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Main code
int main(int, char**)
{
    // 设置DPI感知，使程序适应高分辨率屏幕
    SetProcessDPIAware();
    
    // 获取屏幕分辨率
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    
    // 计算DPI缩放因子
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
    float dpi_scale = dpi / 96.0f; // 96 DPI是标准DPI
    
    // 基准设计分辨率 - 用于计算缩放比例
    float base_width = 1920.0f;
    float base_height = 1080.0f;
    
    // 修改1: 计算窗口位置，使其居中显示
    int window_width = 1920;
    int window_height = 1080;
    int window_x = (screen_width - window_width) / 2;
    int window_y = (screen_height - window_height) / 2;
    
    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    
    // 修改2: 使用计算出的位置和大小创建窗口
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX11 Example", 
                              WS_OVERLAPPEDWINDOW, 
                              window_x, window_y, 
                              window_width, window_height, 
                              nullptr, nullptr, wc.hInstance, nullptr);

    // 修改3: 调整窗口大小以确保客户区域精确为1920x1080
    RECT window_rect, client_rect;
    GetWindowRect(hwnd, &window_rect);
    GetClientRect(hwnd, &client_rect);
    
    int border_width = (window_rect.right - window_rect.left) - client_rect.right;
    int border_height = (window_rect.bottom - window_rect.top) - client_rect.bottom;
    
    // 调整窗口大小，加上边框宽度确保客户区是1920x1080
    ::SetWindowPos(hwnd, NULL, window_x, window_y, 
                  window_width + border_width, 
                  window_height + border_height, 
                  SWP_NOZORDER);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // 修改4: 禁用ini文件，每次都使用代码中的默认值
    io.IniFilename = NULL;
    
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // 改进：更合理的字体缩放设置
    // 如果dpi_scale太大，可能导致文字过大，限制最大缩放
    float font_scale = dpi_scale;
    if (font_scale > 1.5f) font_scale = 1.5f;
    io.FontGlobalScale = font_scale;

    // 保存原始样式，用于重置
    ImGuiStyle style_original = ImGui::GetStyle();
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // 改进：根据dpi进行更合理的样式缩放，并为窗口大小设置优化
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(font_scale);
    
    // 调整某些样式参数以优化小窗口显示
    style.WindowPadding = ImVec2(6.0f * font_scale, 6.0f * font_scale);
    style.ItemSpacing = ImVec2(6.0f * font_scale, 4.0f * font_scale);
    style.ItemInnerSpacing = ImVec2(4.0f * font_scale, 4.0f * font_scale);
    style.FramePadding = ImVec2(4.0f * font_scale, 2.0f * font_scale);
    
    // 保存应用了DPI缩放的基础样式
    style_original = ImGui::GetStyle();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts - 根据DPI调整字体大小
    // 改进：使用更合理的基础字体大小，缩放系数改为font_scale
    float base_font_size = 15.0f;
    float font_size = base_font_size * font_scale;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simsun.ttc", font_size, nullptr, io.Fonts->GetGlyphRangesChineseFull());

    // 程序状态
    bool show_demo_window = false;
    bool show_another_window = false;
    bool show_log_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    
    // 初始化ImPlot
    InitializeImPlot();
    
    // 跟踪窗口是否已最大化
    bool was_maximized = false;
    float ui_scale = 1.0f;  // UI缩放比例，根据窗口大小动态调整
    
    // 添加窗口状态变量
    bool window_state_changed = false;  // 窗口状态是否发生变化
    bool plot_window_opened = false;    // 标记图表窗口是否刚打开
    bool plot3d_window_opened = false;  // 标记3D图表窗口是否刚打开

    // Main loop
    bool done = false;
    while (!done && !g_AppState.wantToQuit)
    {
        // Poll and handle messages
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
            
            // 窗口尺寸变化，设置窗口状态变化标志
            window_state_changed = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        // 检测窗口是否最大化，并计算合适的UI缩放比例
        bool is_maximized = IsZoomed(hwnd);
        float display_width = ImGui::GetIO().DisplaySize.x;
        float display_height = ImGui::GetIO().DisplaySize.y;
        
        // 当窗口最大化状态变化或窗口大小改变时，重新计算UI缩放比例
        if (is_maximized != was_maximized || window_state_changed) {
            // 计算当前窗口尺寸与基准设计尺寸的比例
            float width_ratio = display_width / base_width;
            float height_ratio = display_height / base_height;
            
            // 使用较小的比例作为UI缩放因子，以保持所有内容可见
            ui_scale = width_ratio < height_ratio ? width_ratio : height_ratio;
            
            // 限制缩放范围，避免太大或太小
            if (ui_scale < 0.7f) ui_scale = 0.7f;
            if (ui_scale > 1.5f) ui_scale = 1.5f;
            
            // 重要修复：先重置样式，再应用新缩放，避免缩放累积效应
            ImGui::GetStyle() = style_original;  // 恢复原始样式
            ImGui::GetStyle().ScaleAllSizes(ui_scale);  // 只应用一次缩放
            
            was_maximized = is_maximized;
            window_state_changed = false;  // 重置窗口状态变化标志
        }
        
        // 检测各窗口打开状态
        if (g_AppState.showPlotWindow && !plot_window_opened) {
            plot_window_opened = true;  // 标记窗口刚刚打开
        } else if (!g_AppState.showPlotWindow) {
            plot_window_opened = false;  // 窗口关闭后重置标志
        }
        
        if (g_AppState.show3DPlotWindow && !plot3d_window_opened) {
            plot3d_window_opened = true;  // 标记窗口刚刚打开
        } else if (!g_AppState.show3DPlotWindow) {
            plot3d_window_opened = false;  // 窗口关闭后重置标志
        }

        // 显示导航栏
        ShowNavigationBar();

        // 显示各种弹窗
        if (g_AppState.showAboutWindow)
            ShowAboutWindow(&g_AppState.showAboutWindow);

        if (g_AppState.showPrefsWindow)
            ShowPreferencesWindow(&g_AppState.showPrefsWindow);

        if (g_AppState.showThemeWindow)
            ShowThemeWindow(&g_AppState.showThemeWindow);

        if (g_AppState.showHelpWindow)
            ShowHelpWindow(&g_AppState.showHelpWindow);

        if (g_AppState.showUpdateWindow)
            ShowUpdateCheckWindow(&g_AppState.showUpdateWindow);

        // 显示Demo窗口
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 主菜单窗口 - 改进自适应布局
        {
            // 获取菜单栏高度
            float menu_bar_height = ImGui::GetFrameHeight();
            
            // 改进：使用比例布局而非固定大小
            // 左侧面板占总宽度的20%，但最小不小于240像素，最大不超过350像素
            float window_width = display_width * 0.20f;
            
            // 根据当前UI缩放调整最小和最大宽度限制
            float min_width = 240.0f * ui_scale;
            float max_width = 350.0f * ui_scale;
            
            window_width = (window_width < min_width) ? min_width : window_width;
            window_width = (window_width > max_width) ? max_width : window_width;
            float window_height = display_height - menu_bar_height;
            
            // 设置窗口位置和大小 - 位置在菜单栏下方
            ImGui::SetNextWindowPos(ImVec2(0, menu_bar_height), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(window_width, window_height), ImGuiCond_Always);
            
            // 设置窗口标志：不可移动、不可调整大小、不可折叠、无标题栏
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoResize |
                                          ImGuiWindowFlags_NoCollapse |
                                          ImGuiWindowFlags_NoTitleBar;

            // 启用垂直滚动条，确保内容超出时可以滚动查看
            window_flags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;

            ImGui::Begin(u8"Main", nullptr, window_flags);

            // 添加更多内容以测试窗口是否能容纳所有文字
            ImGui::Text(u8"主控制面板");
            ImGui::Separator();
            
            ImGui::Text(u8"这是一些有用的文本信息。");
            ImGui::TextWrapped(u8"当前显示分辨率: %.0f x %.0f", display_width, display_height);
            ImGui::TextWrapped(u8"缩放比例: %.2f (UI: %.2f)", font_scale, ui_scale);
            
            ImGui::Separator();
            
            ImGui::Checkbox(u8"演示窗口", &show_demo_window);
            ImGui::Checkbox(u8"日志窗口", &show_log_window);
            
            ImGui::Separator();

            // 图表窗口按钮
            if (ImGui::Button(u8"打开图表", ImVec2(-1, 0)))
                g_AppState.showPlotWindow = true;

            if (ImGui::Button(u8"打开3D图表", ImVec2(-1, 0)))
                g_AppState.show3DPlotWindow = true;
            
            ImGui::Separator();

            // 显示窗口信息
            ImGui::TextWrapped(u8"窗口信息:");
             // 重新获取当前窗口的实际尺寸，而不是使用计算值
            ImVec2 window_current_size = ImGui::GetWindowSize();
            ImVec2 window_content_size = ImGui::GetContentRegionAvail();
            
            // 显示实时更新的窗口信息
            ImGui::TextWrapped(u8"窗口大小: %.1f x %.1f", window_current_size.x, window_current_size.y);
            ImGui::TextWrapped(u8"内容区域: %.1f x %.1f", window_content_size.x, window_content_size.y);
            ImGui::TextWrapped(u8"菜单栏高度: %.1f", menu_bar_height);
            ImGui::TextWrapped(u8"窗口最大化: %s", is_maximized ? "是" : "否");
            
            // 显示实际屏幕分辨率
            ImGui::TextWrapped(u8"屏幕分辨率: %.1f x %.1f", display_width, display_height);
            
            ImGui::End();
        }

        // 日志窗口 - 改进自适应布局
        if (show_log_window)
        {
            // 获取菜单栏高度
            float menu_bar_height = ImGui::GetFrameHeight();
            
            // 改进：使用比例布局，确保在不同分辨率下都有合适的大小
            // 计算左侧面板宽度（与上面主菜单窗口一致）
            float left_panel_width = display_width * 0.20f;
            
            // 根据当前UI缩放调整最小和最大宽度限制
            float min_width = 240.0f * ui_scale;
            float max_width = 350.0f * ui_scale;
            
            left_panel_width = (left_panel_width < min_width) ? min_width : left_panel_width;
            left_panel_width = (left_panel_width > max_width) ? max_width : left_panel_width;
            
            // 日志窗口计算
            float log_pos_x = left_panel_width;
            float log_pos_y = menu_bar_height + display_height * 0.6f; // 位于垂直60%的位置
            float log_width = display_width - left_panel_width;
            float log_height = display_height - log_pos_y;
            
            // 调整最小高度，确保日志窗口有足够的显示空间
            float min_log_height = 180.0f * ui_scale;
            if (log_height < min_log_height) log_height = min_log_height;
            
            // 确保日志窗口不会超出屏幕边界
            if (log_pos_y + log_height > display_height) {
                log_height = display_height - log_pos_y - 10; // 留10像素边距
            }
            
            // 设置窗口位置和大小
            ImGui::SetNextWindowPos(ImVec2(log_pos_x, log_pos_y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(log_width, log_height), ImGuiCond_Always);
            
            // 设置窗口标志：不可移动、不可调整大小、总是显示垂直滚动条
            ImGuiWindowFlags log_window_flags = ImGuiWindowFlags_NoMove | 
                                               ImGuiWindowFlags_NoResize | 
                                               ImGuiWindowFlags_NoCollapse |
                                               ImGuiWindowFlags_AlwaysVerticalScrollbar;
            
            // 创建日志窗口并显示其内容
            if (ImGui::Begin(u8"日志窗口", &show_log_window, log_window_flags)) {
                ShowLogWindow(nullptr);
            }
            ImGui::End();
        }

        // 如果图表窗口打开，也需要适应分辨率
        if (g_AppState.showPlotWindow) {
            float menu_bar_height = ImGui::GetFrameHeight();
            
            // 改进：图表窗口位置和大小计算
            float left_panel_width = display_width * 0.20f;
            
            // 根据当前UI缩放调整最小和最大宽度限制
            float min_width = 240.0f * ui_scale;
            float max_width = 350.0f * ui_scale;
            
            left_panel_width = (left_panel_width < min_width) ? min_width : left_panel_width;
            left_panel_width = (left_panel_width > max_width) ? max_width : left_panel_width;
            
            float plot_pos_x = left_panel_width;
            float plot_pos_y = menu_bar_height;
            float plot_width = display_width - left_panel_width;
            float plot_height = display_height * 0.6f - menu_bar_height;
            
            // 修复：窗口刚打开或窗口状态变化时，总是使用当前计算的位置和大小
            ImGuiCond position_cond = plot_window_opened ? ImGuiCond_Always : ImGuiCond_Always;
            ImGuiCond size_cond = plot_window_opened ? ImGuiCond_Always : ImGuiCond_Always;
            
            ImGui::SetNextWindowPos(ImVec2(plot_pos_x, plot_pos_y), position_cond);
            ImGui::SetNextWindowSize(ImVec2(plot_width, plot_height), size_cond);
            
            // 窗口已打开，重置标志
            plot_window_opened = false;
            
            ShowPlotWindow(&g_AppState.showPlotWindow);
        }

        // 如果3D图表窗口打开，也需要适应分辨率
        if (g_AppState.show3DPlotWindow) {
            float menu_bar_height = ImGui::GetFrameHeight();
            
            // 改进：3D图表窗口计算方式
            float left_panel_width = display_width * 0.20f;
            
            // 根据当前UI缩放调整最小和最大宽度限制
            float min_width = 240.0f * ui_scale;
            float max_width = 350.0f * ui_scale;
            
            left_panel_width = (left_panel_width < min_width) ? min_width : left_panel_width;
            left_panel_width = (left_panel_width > max_width) ? max_width : left_panel_width;
            
            // 初始窗口位置调整，比例计算
            float plot3d_pos_x = (display_width - left_panel_width) * 0.1f + left_panel_width;
            float plot3d_pos_y = menu_bar_height + display_height * 0.1f;
            float plot3d_width = display_width * 0.7f;
            float plot3d_height = display_height * 0.7f;
            
            // 修复：窗口刚打开或窗口状态变化时，总是使用当前计算的位置和大小
            ImGuiCond position_cond = plot3d_window_opened ? ImGuiCond_Always : ImGuiCond_Always;
            ImGuiCond size_cond = plot3d_window_opened ? ImGuiCond_Always : ImGuiCond_Always;
            
            ImGui::SetNextWindowPos(ImVec2(plot3d_pos_x, plot3d_pos_y), position_cond);
            ImGui::SetNextWindowSize(ImVec2(plot3d_width, plot3d_height), size_cond);
            
            // 窗口已打开，重置标志
            plot3d_window_opened = false;
            
            DemoMeshPlots(&g_AppState.show3DPlotWindow);
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    CleanupImPlot();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}