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

#include "../include/app.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot3d.h"
#include <fstream>
#include <sys/stat.h>

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

// 窗口约束函数
void ConstrainWindowsToMainViewport() {
    // 获取主视口(应用程序窗口)的大小和位置
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 viewport_min = viewport->Pos;
    ImVec2 viewport_max = ImVec2(viewport->Pos.x + viewport->Size.x,
        viewport->Pos.y + viewport->Size.y);

    // 获取所有活动窗口
    ImGuiContext& g = *ImGui::GetCurrentContext();
    for (int i = 0; i < g.Windows.Size; i++) {
        ImGuiWindow* window = g.Windows[i];
        
        // 跳过不可见、折叠或不需要限制的窗口
        if (!window->WasActive || window->Hidden || window->Collapsed || 
            (window->Flags & ImGuiWindowFlags_ChildWindow) ||
            (window->Flags & ImGuiWindowFlags_Tooltip) ||
            (window->Flags & ImGuiWindowFlags_Popup)) {
            continue;
        }

        // 获取窗口尺寸和位置
        ImVec2 window_pos = window->Pos;
        ImVec2 window_size = window->Size;

        // 确定窗口的最小可见区域 - 至少保证标题栏可见
        float min_visible = window->TitleBarHeight > 0.0f ?
            window->TitleBarHeight : 30.0f;

        // 约束窗口位置
        bool position_changed = false;

        // X轴约束 - 确保窗口不会完全超出左侧或右侧
        if (window_pos.x + window_size.x < viewport_min.x + min_visible) {
            window_pos.x = viewport_min.x + min_visible - window_size.x;
            position_changed = true;
        }
        if (window_pos.x > viewport_max.x - min_visible) {
            window_pos.x = viewport_max.x - min_visible;
            position_changed = true;
        }

        // Y轴约束 - 确保窗口不会完全超出顶部或底部
        if (window_pos.y + window_size.y < viewport_min.y + min_visible) {
            window_pos.y = viewport_min.y + min_visible - window_size.y;
            position_changed = true;
        }
        if (window_pos.y > viewport_max.y - min_visible) {
            window_pos.y = viewport_max.y - min_visible;
            position_changed = true;
        }

        // 如果位置需要调整，设置新位置
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

// 获取文件修改时间
std::time_t GetFileLastModifiedTime(const std::string& filePath) {
    struct stat result;
    if (stat(filePath.c_str(), &result) == 0) {
        return result.st_mtime;
    }
    return 0;
}

// Direct3D 相关函数
bool CreateDeviceD3D(HWND hWnd) {
    // Setup swap chain
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
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
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

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}