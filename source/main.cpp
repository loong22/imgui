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
    
    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX11 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1920, 1080, nullptr, nullptr, wc.hInstance, nullptr);

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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // 改进：更合理的字体缩放设置
    // 如果dpi_scale太大，可能导致文字过大，限制最大缩放
    float font_scale = dpi_scale;
    if (font_scale > 1.5f) font_scale = 1.5f;
    io.FontGlobalScale = font_scale;

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

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts - 根据DPI调整字体大小
    // 改进：使用更合理的基础字体大小，缩放系数改为font_scale
    float base_font_size = 14.0f;
    float font_size = base_font_size * font_scale;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simsun.ttc", font_size, nullptr, io.Fonts->GetGlyphRangesChineseFull());

    // 程序状态
    bool show_demo_window = false;
    bool show_another_window = false;
    bool show_log_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    
    // 初始化ImPlot
    InitializeImPlot();

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
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

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
            // 获取当前显示器大小
            float display_width = ImGui::GetIO().DisplaySize.x;
            float display_height = ImGui::GetIO().DisplaySize.y;
            
            // 获取菜单栏高度
            float menu_bar_height = ImGui::GetFrameHeight();
            
            // 改进：使用比例布局而非固定大小
            // 左侧面板占总宽度的20%，但最小不小于240像素，最大不超过350像素
            float window_width = display_width * 0.20f;
            window_width = (window_width < 240.0f) ? 240.0f : window_width;
            window_width = (window_width > 350.0f) ? 350.0f : window_width;
            float window_height = display_height - menu_bar_height;
            
            // 设置窗口位置和大小 - 位置在菜单栏下方
            ImGui::SetNextWindowPos(ImVec2(0, menu_bar_height), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(window_width, window_height), ImGuiCond_FirstUseEver);
            
            // 设置窗口标志：不可移动、不可调整大小、不可折叠、无标题栏
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove |
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
            ImGui::TextWrapped(u8"缩放比例: %.2f", font_scale);
            
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
            
            // 添加一些测试控件
            ImGui::Text(u8"测试控件:");
            static float test_float = 0.5f;
            ImGui::SliderFloat(u8"测试滑块", &test_float, 0.0f, 1.0f);
            
            static int test_int = 50;
            ImGui::SliderInt(u8"测试整数", &test_int, 0, 100);
            
            static bool test_bool = true;
            ImGui::Checkbox(u8"测试复选框", &test_bool);
            
            ImGui::Separator();
            
            // 显示窗口信息
            ImGui::TextWrapped(u8"窗口信息:");
            ImGui::TextWrapped(u8"窗口大小: %.1f x %.1f", window_width, window_height);
            ImGui::TextWrapped(u8"菜单栏高度: %.1f", menu_bar_height);
            
            ImGui::End();
        }

        // 日志窗口 - 改进自适应布局
        if (show_log_window)
        {
            // 获取当前显示器大小
            float display_width = ImGui::GetIO().DisplaySize.x;
            float display_height = ImGui::GetIO().DisplaySize.y;
            float menu_bar_height = ImGui::GetFrameHeight();
            
            // 改进：使用比例布局，确保在不同分辨率下都有合适的大小
            // 计算左侧面板宽度（与上面主菜单窗口一致）
            float left_panel_width = display_width * 0.20f;
            left_panel_width = (left_panel_width < 240.0f) ? 240.0f : left_panel_width;
            left_panel_width = (left_panel_width > 350.0f) ? 350.0f : left_panel_width;
            
            // 日志窗口计算
            float log_pos_x = left_panel_width;
            float log_pos_y = menu_bar_height + display_height * 0.6f; // 位于垂直60%的位置
            float log_width = display_width - left_panel_width;
            float log_height = display_height - log_pos_y;
            
            // 调整最小高度，确保日志窗口有足够的显示空间
            if (log_height < 180.0f) log_height = 180.0f;
            
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
            float display_width = ImGui::GetIO().DisplaySize.x;
            float display_height = ImGui::GetIO().DisplaySize.y;
            float menu_bar_height = ImGui::GetFrameHeight();
            
            // 改进：图表窗口位置和大小计算
            float left_panel_width = display_width * 0.20f;
            left_panel_width = (left_panel_width < 240.0f) ? 240.0f : left_panel_width;
            left_panel_width = (left_panel_width > 350.0f) ? 350.0f : left_panel_width;
            
            float plot_pos_x = left_panel_width;
            float plot_pos_y = menu_bar_height;
            float plot_width = display_width - left_panel_width;
            float plot_height = display_height * 0.6f - menu_bar_height;
            
            ImGui::SetNextWindowPos(ImVec2(plot_pos_x, plot_pos_y), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(plot_width, plot_height), ImGuiCond_FirstUseEver);
            
            ShowPlotWindow(&g_AppState.showPlotWindow);
        }

        // 如果3D图表窗口打开，也需要适应分辨率
        if (g_AppState.show3DPlotWindow) {
            float display_width = ImGui::GetIO().DisplaySize.x;
            float display_height = ImGui::GetIO().DisplaySize.y;
            float menu_bar_height = ImGui::GetFrameHeight();
            
            // 改进：3D图表窗口计算方式
            float left_panel_width = display_width * 0.20f;
            left_panel_width = (left_panel_width < 240.0f) ? 240.0f : left_panel_width;
            left_panel_width = (left_panel_width > 350.0f) ? 350.0f : left_panel_width;
            
            // 初始窗口位置调整，比例计算
            float plot3d_pos_x = (display_width - left_panel_width) * 0.1f + left_panel_width;
            float plot3d_pos_y = menu_bar_height + display_height * 0.1f;
            float plot3d_width = display_width * 0.7f;
            float plot3d_height = display_height * 0.7f;
            
            ImGui::SetNextWindowPos(ImVec2(plot3d_pos_x, plot3d_pos_y), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(plot3d_width, plot3d_height), ImGuiCond_FirstUseEver);
            
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