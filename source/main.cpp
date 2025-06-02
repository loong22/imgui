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
#include "imgui_internal.h"  // 添加这个头文件以使用ImGuiWindow和FindWindowByName
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
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"SU2GUI", 
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
    
    // 启用INI文件保存窗口设置
    // io.IniFilename = NULL;  // 注释掉这行以启用INI文件
    
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
    bool plot_window_opened = false;    // 标记图表窗口是否刚打开
    bool plot3d_window_opened = false;  // 标记3D图表窗口是否刚打开
    
    // 添加缺失的变量声明
    ImVec2 last_main_window_pos(0, 0);
    ImVec2 last_main_window_size(0, 0);

    // Main loop
    bool done = false;
    
    // 添加用于追踪分辨率变化的变量
    static float last_display_width = 0.0f;
    static float last_display_height = 0.0f;
    static float last_resolution_scale = 1.0f;
    static ImGuiStyle original_style = ImGui::GetStyle(); // 保存原始样式
    
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
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        // 检测窗口是否最大化，并计算合适的UI缩放比例
        bool is_maximized = IsZoomed(hwnd);
        float display_width = ImGui::GetIO().DisplaySize.x;
        float display_height = ImGui::GetIO().DisplaySize.y;
        
        // 改进的分辨率缩放计算
        float resolution_scale = 1.0f;
        if (display_width >= 3840 && display_height >= 2160) {
            // 4K分辨率
            resolution_scale = 2.0f;
        } else if (display_width >= 2560 && display_height >= 1440) {
            // 2K分辨率
            resolution_scale = 1.5f;
        } else if (display_width >= 1920 && display_height >= 1080) {
            // 1080p分辨率
            resolution_scale = 1.0f;
        } else {
            // 较低分辨率，适当缩小
            resolution_scale = display_width / 1920.0f;
            if (resolution_scale < 0.7f) resolution_scale = 0.7f;
        }
        
        // 实时检测分辨率变化并更新UI缩放
        bool resolution_changed = false;
        if (last_display_width != display_width || 
            last_display_height != display_height || 
            last_resolution_scale != resolution_scale) {
            
            resolution_changed = true;
            last_display_width = display_width;
            last_display_height = display_height;
            last_resolution_scale = resolution_scale;
            
            // 重新计算UI缩放
            float width_ratio = display_width / base_width;
            float height_ratio = display_height / base_height;
            ui_scale = width_ratio < height_ratio ? width_ratio : height_ratio;
            
            // 结合分辨率缩放
            ui_scale *= resolution_scale;
            
            if (ui_scale < 0.7f) ui_scale = 0.7f;
            if (ui_scale > 3.0f) ui_scale = 3.0f;
            
            // 重置样式到原始状态，然后应用新的缩放
            ImGui::GetStyle() = original_style;
            ImGui::GetStyle().ScaleAllSizes(ui_scale);
            
            // 输出调试信息
            printf("分辨率变化检测: %.0fx%.0f, 缩放比例: %.2f\n", 
                   display_width, display_height, ui_scale);
        }

        // 检测各窗口打开状态
        if (g_AppState.showPlotWindow && !plot_window_opened) {
            plot_window_opened = true;
        } else if (!g_AppState.showPlotWindow) {
            plot_window_opened = false;
        }
        
        if (g_AppState.show3DPlotWindow && !plot3d_window_opened) {
            plot3d_window_opened = true;
        } else if (!g_AppState.show3DPlotWindow) {
            plot3d_window_opened = false;
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

        // 主菜单窗口 - 改进的分辨率自适应
        {
            // 获取菜单栏高度
            float menu_bar_height = ImGui::GetFrameHeight();
            
            // 根据分辨率计算窗口大小
            float window_width = display_width * 0.18f;
            float min_width = 200.0f * resolution_scale;
            float max_width = 400.0f * resolution_scale;
            
            window_width = (window_width < min_width) ? min_width : window_width;
            window_width = (window_width > max_width) ? max_width : window_width;
            float window_height = display_height - menu_bar_height;
            
            // 如果分辨率发生变化，重新设置窗口大小
            ImGuiCond window_cond = resolution_changed ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
            ImGui::SetNextWindowPos(ImVec2(0, menu_bar_height), window_cond);
            ImGui::SetNextWindowSize(ImVec2(window_width, window_height), window_cond);
            
            // 设置窗口标志
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse |
                                          ImGuiWindowFlags_NoTitleBar |
                                          ImGuiWindowFlags_AlwaysVerticalScrollbar;

            ImGui::Begin(u8"Main", nullptr, window_flags);
            
            // 记录当前实际的窗口位置和大小
            ImVec2 current_main_pos = ImGui::GetWindowPos();
            ImVec2 current_main_size = ImGui::GetWindowSize();
          
            // 添加更多内容以测试窗口是否能容纳所有文字
            ImGui::Text(u8"主控制面板");
            ImGui::Separator();
            
            ImGui::Text(u8"这是一些有用的文本信息。");
            ImGui::TextWrapped(u8"当前显示分辨率: %.0f x %.0f", display_width, display_height);
            ImGui::TextWrapped(u8"缩放比例: %.2f (UI: %.2f)", font_scale, ui_scale);
            ImGui::TextWrapped(u8"分辨率缩放: %.2f", resolution_scale);
            
            ImGui::Separator();
            
            ImGui::Checkbox(u8"演示窗口", &show_demo_window);
            ImGui::Checkbox(u8"日志窗口", &show_log_window);
            
            ImGui::Separator();

            // 图表窗口按钮 - 根据UI缩放调整按钮高度
            float button_height = 30.0f * ui_scale;
            if (ImGui::Button(u8"打开图表", ImVec2(-1, button_height)))
                g_AppState.showPlotWindow = true;

            if (ImGui::Button(u8"打开3D图表", ImVec2(-1, button_height)))
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

        // 日志窗口 - 改进的分辨率自适应
        if (show_log_window)
        {
            float menu_bar_height = ImGui::GetFrameHeight();
            
            // 根据分辨率计算布局
            float left_panel_width = display_width * 0.18f;
            float min_width = 200.0f * resolution_scale;
            float max_width = 400.0f * resolution_scale;
            
            left_panel_width = (left_panel_width < min_width) ? min_width : left_panel_width;
            left_panel_width = (left_panel_width > max_width) ? max_width : left_panel_width;
            
            float log_pos_x = left_panel_width;
            float log_pos_y = menu_bar_height + display_height * 0.55f;
            float log_width = display_width - left_panel_width;
            float log_height = display_height - log_pos_y;
            
            float min_log_height = 150.0f * resolution_scale;
            if (log_height < min_log_height) {
                log_height = min_log_height;
                log_pos_y = display_height - log_height - 10;
                if (log_pos_y < menu_bar_height) {
                    log_pos_y = menu_bar_height;
                    log_height = display_height - menu_bar_height - 10;
                }
            }
            
            // 如果分辨率发生变化，重新设置窗口大小
            ImGuiCond window_cond = resolution_changed ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
            ImGui::SetNextWindowPos(ImVec2(log_pos_x, log_pos_y), window_cond);
            ImGui::SetNextWindowSize(ImVec2(log_width, log_height), window_cond);
            
            ImGuiWindowFlags log_window_flags = ImGuiWindowFlags_NoCollapse |
                                               ImGuiWindowFlags_AlwaysVerticalScrollbar;
            
            if (ImGui::Begin(u8"日志窗口", &show_log_window, log_window_flags)) {
                ShowLogWindow(nullptr);
            }
            ImGui::End();
        }

        // 图表窗口 - 改进的分辨率自适应
        if (g_AppState.showPlotWindow) {
            float menu_bar_height = ImGui::GetFrameHeight();
            
            // 根据分辨率计算默认位置和大小
            float left_panel_width = display_width * 0.18f;
            float min_width = 200.0f * resolution_scale;
            float max_width = 400.0f * resolution_scale;
            
            left_panel_width = (left_panel_width < min_width) ? min_width : left_panel_width;
            left_panel_width = (left_panel_width > max_width) ? max_width : left_panel_width;
            
            float plot_pos_x = left_panel_width;
            float plot_pos_y = menu_bar_height;
            float plot_width = (display_width - left_panel_width) * 0.65f;
            float plot_height = display_height * 0.45f - menu_bar_height;
            
            float min_plot_width = 350.0f * resolution_scale;
            float min_plot_height = 250.0f * resolution_scale;
            plot_width = (plot_width < min_plot_width) ? min_plot_width : plot_width;
            plot_height = (plot_height < min_plot_height) ? min_plot_height : plot_height;
            
            // 根据窗口状态和分辨率变化决定设置条件
            ImGuiCond window_cond = ImGuiCond_Appearing;
            if (resolution_changed) {
                window_cond = ImGuiCond_Always;
            }
            
            if (plot_window_opened || resolution_changed) {
                ImGui::SetNextWindowPos(ImVec2(plot_pos_x, plot_pos_y), window_cond);
                ImGui::SetNextWindowSize(ImVec2(plot_width, plot_height), window_cond);
            }
            
            ShowPlotWindow(&g_AppState.showPlotWindow);
        }

        // 3D图表窗口 - 改进的分辨率自适应
        if (g_AppState.show3DPlotWindow) {
            float menu_bar_height = ImGui::GetFrameHeight();
            
            // 根据分辨率计算默认位置和大小
            float left_panel_width = display_width * 0.18f;
            float min_width = 200.0f * resolution_scale;
            float max_width = 400.0f * resolution_scale;
            
            left_panel_width = (left_panel_width < min_width) ? min_width : left_panel_width;
            left_panel_width = (left_panel_width > max_width) ? max_width : left_panel_width;
            
            float plot3d_pos_x = (display_width - left_panel_width) * 0.12f + left_panel_width;
            float plot3d_pos_y = menu_bar_height + display_height * 0.08f;
            float plot3d_width = display_width * 0.45f;
            float plot3d_height = display_height * 0.55f;
            
            float min_plot3d_width = 400.0f * resolution_scale;
            float min_plot3d_height = 300.0f * resolution_scale;
            plot3d_width = (plot3d_width < min_plot3d_width) ? min_plot3d_width : plot3d_width;
            plot3d_height = (plot3d_height < min_plot3d_height) ? min_plot3d_height : plot3d_height;
            
            // 根据窗口状态和分辨率变化决定设置条件
            ImGuiCond window_cond = ImGuiCond_Appearing;
            if (resolution_changed) {
                window_cond = ImGuiCond_Always;
            }
            
            if (plot3d_window_opened || resolution_changed) {
                ImGui::SetNextWindowPos(ImVec2(plot3d_pos_x, plot3d_pos_y), window_cond);
                ImGui::SetNextWindowSize(ImVec2(plot3d_width, plot3d_height), window_cond);
            }
            
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