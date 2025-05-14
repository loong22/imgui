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
    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX11 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

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

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simsun.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());

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

        // 约束窗口位置 @TODO 不起作用, 可能是因为窗口没有被创建
        // ConstrainWindowsToMainViewport();

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

        // 显示图表窗口
        if (g_AppState.showPlotWindow)
            ShowPlotWindow(&g_AppState.showPlotWindow);

        if (g_AppState.show3DPlotWindow)
            DemoMeshPlots(&g_AppState.show3DPlotWindow);

        // 显示Demo窗口
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 主菜单窗口
        {
            static float f = 0.0f;
            static int counter = 0;
            
            // 设置窗口位置和大小
            ImGui::SetNextWindowPos(ImVec2(0, 23), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(442, 400), ImGuiCond_FirstUseEver);
            
            // 设置窗口标志：不可移动，但可以调整大小
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove;

            ImGui::Begin(u8"主菜单", nullptr, window_flags);

            ImGui::Text("This is some useful text.");               
            ImGui::Checkbox("Demo Window", &show_demo_window);      
            ImGui::Checkbox("Another Window", &show_another_window);
            ImGui::Checkbox("Log Window", &show_log_window);
            
            ImGui::Separator();

            // 图表窗口按钮
            if (ImGui::Button(u8"打开图表"))
                g_AppState.showPlotWindow = true;
            
            if (ImGui::Button(u8"打开3D图表"))
                g_AppState.show3DPlotWindow = true;
            
            ImGui::Separator();
            
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            
            ImGui::ColorEdit3("clear color", (float*)&clear_color); 

            if (ImGui::Button("Button"))                            
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 另一个示例窗口
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   
            ImGui::Text("Hello from another window!");
            ImGui::Text(u8"中文字体测试！！！");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // 日志窗口
        if (show_log_window)
        {
            // 设置窗口位置和大小
            ImGui::SetNextWindowSize(ImVec2(886, 286), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 886, ImGui::GetIO().DisplaySize.y - 286), ImGuiCond_FirstUseEver);
            
            // 创建日志窗口并显示其内容
            if (ImGui::Begin(u8"日志窗口", &show_log_window)) {
                ShowLogWindow(nullptr);
            }
            ImGui::End();
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