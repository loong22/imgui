#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <fstream>
#include <imgui_internal.h>
#include <sstream>
#include <ctime>    // 用于时间相关操作
#include <sys/stat.h>  // 用于获取文件信息
#include <vector>   // 用于缓冲区

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// 程序状态数据
struct AppState {
    bool showAboutWindow = false;      // 是否显示"关于"窗口
    bool showPrefsWindow = false;      // 是否显示"首选项"窗口
    bool showThemeWindow = false;      // 是否显示"颜色主题"窗口
    bool showHelpWindow = false;       // 是否显示"帮助"窗口
    bool showUpdateWindow = false;     // 是否显示"检查更新"窗口

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

static AppState g_AppState;

// 导航栏相关函数声明
void ShowNavigationBar();
void ShowFileMenu();
void ShowSettingsMenu();
void ShowHelpMenu();
void ShowAboutWindow(bool* p_open);
void ShowPreferencesWindow(bool* p_open);
void ShowThemeWindow(bool* p_open);
void ShowHelpWindow(bool* p_open);
void ShowUpdateCheckWindow(bool* p_open);
bool SaveFile(const std::string& filePath, const std::string& content);
bool LoadFile(const std::string& filePath, std::string& content);

// 添加到全局函数声明部分
void ConstrainWindowsToMainViewport();

// 日志相关函数声明
void ShowLogWindow(bool* p_open);
bool LoadLogFileIncremental(const std::string& filePath, std::string& content, std::streampos& lastPos);
void OpenLogFile();
void ClearLogContent();

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
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
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simsun.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());

    // Our state
    //示例窗口
    bool show_demo_window = true;
    //示例窗口
    bool show_another_window = false;
    //日志窗口
    bool show_log_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done && !g_AppState.wantToQuit)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
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

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
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

        // 新增: 约束窗口位置确保其不会超出主窗口边界  @TODO:不起作用!
        ConstrainWindowsToMainViewport();

        // 显示导航栏（始终显示且不可关闭）
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

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin(u8"主菜单");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);
            ImGui::Checkbox("Log Window", &show_log_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            ImGui::Text(u8"中文字体测试！！！");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        if (show_log_window)
        {
            ShowLogWindow(&show_log_window);
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// 导航栏实现
void ShowNavigationBar()
{
    // 使用BeginMainMenuBar创建主菜单栏
    if (ImGui::BeginMainMenuBar())
    {
        // 文件菜单
        if (ImGui::BeginMenu(u8"文件"))
        {
            ShowFileMenu();
            ImGui::EndMenu();
        }

        // 设置菜单
        if (ImGui::BeginMenu(u8"设置"))
        {
            ShowSettingsMenu();
            ImGui::EndMenu();
        }

        // 帮助菜单
        if (ImGui::BeginMenu(u8"帮助"))
        {
            ShowHelpMenu();
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

// 文件菜单实现
// 修改ShowFileMenu函数
void ShowFileMenu()
{
    if (ImGui::MenuItem(u8"新建", "Ctrl+N"))
    {
        g_AppState.currentFilePath.clear();
        g_AppState.fileContent.clear();
    }

    if (ImGui::MenuItem(u8"打开", "Ctrl+O"))
    {
        OPENFILENAMEA ofn;
        char szFile[260] = { 0 };

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();  // 获取活动窗口
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "文本文件\0*.txt\0所有文件\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn) == TRUE) {
            std::string filePath = ofn.lpstrFile;
            if (LoadFile(filePath, g_AppState.fileContent)) {
                g_AppState.currentFilePath = filePath;
            }
        }
    }

    if (ImGui::MenuItem(u8"保存", "Ctrl+S"))
    {
        if (!g_AppState.currentFilePath.empty())
        {
            SaveFile(g_AppState.currentFilePath, g_AppState.fileContent);
        }
        else
        {
            OPENFILENAMEA ofn;
            char szFile[260] = { 0 };
            strcpy(szFile, "untitled.txt");

            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = GetActiveWindow();  // 获取活动窗口
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "文本文件\0*.txt\0所有文件\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
            ofn.lpstrDefExt = "txt";

            if (GetSaveFileNameA(&ofn) == TRUE) {
                std::string filePath = ofn.lpstrFile;
                if (SaveFile(filePath, g_AppState.fileContent)) {
                    g_AppState.currentFilePath = filePath;
                }
            }
        }
    }

    ImGui::Separator();

    if (ImGui::MenuItem(u8"退出", "Alt+F4"))
    {
        g_AppState.wantToQuit = true;
    }
}

// 设置菜单实现
void ShowSettingsMenu()
{
    if (ImGui::MenuItem(u8"首选项"))
    {
        g_AppState.showPrefsWindow = true;
    }

    if (ImGui::MenuItem(u8"颜色主题"))
    {
        g_AppState.showThemeWindow = true;
    }

    if (ImGui::BeginMenu(u8"高级设置"))
    {
        if (ImGui::MenuItem(u8"选项 1"))
        {
            // 选项1的实现
            ImGui::OpenPopup(u8"高级选项1");
        }

        if (ImGui::MenuItem(u8"选项 2"))
        {
            // 选项2的实现
            ImGui::OpenPopup(u8"高级选项2");
        }

        // 高级选项1弹窗
        if (ImGui::BeginPopupModal(u8"高级选项1", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(u8"这是高级选项1的设置窗口");

            static bool option1 = false;
            static bool option2 = true;
            static float value = 0.5f;

            ImGui::Checkbox(u8"启用选项1", &option1);
            ImGui::Checkbox(u8"启用选项2", &option2);
            ImGui::SliderFloat(u8"设置值", &value, 0.0f, 1.0f);

            if (ImGui::Button(u8"确定", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // 高级选项2弹窗
        if (ImGui::BeginPopupModal(u8"高级选项2", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(u8"这是高级选项2的设置窗口");

            static int selection = 0;
            ImGui::RadioButton(u8"模式1", &selection, 0);
            ImGui::RadioButton(u8"模式2", &selection, 1);
            ImGui::RadioButton(u8"模式3", &selection, 2);

            if (ImGui::Button(u8"确定", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndMenu();
    }
}

// 帮助菜单实现
void ShowHelpMenu()
{
    if (ImGui::MenuItem(u8"查看文档"))
    {
        g_AppState.showHelpWindow = true;
    }

    if (ImGui::MenuItem(u8"检查更新"))
    {
        g_AppState.showUpdateWindow = true;
    }

    if (ImGui::MenuItem(u8"关于"))
    {
        g_AppState.showAboutWindow = true;
    }
}

// 关于窗口实现
void ShowAboutWindow(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(u8"关于", p_open, ImGuiWindowFlags_NoResize))
    {
        ImGui::Text(u8"Demo GUI 应用程序");
        ImGui::Separator();
        ImGui::Text(u8"版本: 1.0.0");
        ImGui::Text(u8"作者: 示例开发者");
        ImGui::Text(u8"版权所有 © 2025");
        ImGui::Separator();
        ImGui::Text(u8"本程序使用了以下库:");
        ImGui::BulletText("Dear ImGui");
        ImGui::BulletText("DirectX 11");

        ImGui::Separator();
        if (ImGui::Button(u8"确定", ImVec2(120, 0)))
        {
            *p_open = false;
        }
    }
    ImGui::End();
}

// 首选项窗口实现
void ShowPreferencesWindow(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(u8"首选项", p_open))
    {
        static int tab_index = 0;
        ImGui::BeginTabBar("PreferencesTabBar");

        // 常规设置选项卡
        if (ImGui::BeginTabItem(u8"常规"))
        {
            static bool autoSave = true;
            static int autoSaveInterval = 5;
            static bool showToolbar = true;
            static bool showStatusBar = true;

            ImGui::Checkbox(u8"启用自动保存", &autoSave);
            if (autoSave)
            {
                ImGui::SliderInt(u8"自动保存间隔 (分钟)", &autoSaveInterval, 1, 30);
            }

            ImGui::Separator();
            ImGui::Checkbox(u8"显示工具栏", &showToolbar);
            ImGui::Checkbox(u8"显示状态栏", &showStatusBar);

            ImGui::EndTabItem();
        }

        // 编辑器设置选项卡
        if (ImGui::BeginTabItem(u8"编辑器"))
        {
            static bool wordWrap = true;
            static bool showLineNumbers = true;
            static int tabSize = 4;
            static bool insertSpaces = true;

            ImGui::Checkbox(u8"自动换行", &wordWrap);
            ImGui::Checkbox(u8"显示行号", &showLineNumbers);
            ImGui::SliderInt(u8"Tab 大小", &tabSize, 1, 8);
            ImGui::Checkbox(u8"Tab键插入空格", &insertSpaces);

            ImGui::EndTabItem();
        }

        // 主题设置选项卡
        if (ImGui::BeginTabItem(u8"主题"))
        {
            ImGui::Checkbox(u8"使用深色主题", &g_AppState.isDarkTheme);

            if (ImGui::Button(u8"应用主题"))
            {
                if (g_AppState.isDarkTheme)
                    ImGui::StyleColorsDark();
                else
                    ImGui::StyleColorsLight();
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();

        ImGui::Separator();
        if (ImGui::Button(u8"保存设置", ImVec2(120, 0)))
        {
            // 这里应该保存设置到配置文件
            *p_open = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"取消", ImVec2(120, 0)))
        {
            *p_open = false;
        }
    }
    ImGui::End();
}

// 主题窗口实现
void ShowThemeWindow(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(u8"颜色主题", p_open))
    {
        static int theme_index = g_AppState.isDarkTheme ? 0 : 1;
        ImGui::Text(u8"选择颜色主题:");

        if (ImGui::RadioButton(u8"深色主题", &theme_index, 0))
        {
            g_AppState.isDarkTheme = true;
            ImGui::StyleColorsDark();
        }

        if (ImGui::RadioButton(u8"浅色主题", &theme_index, 1))
        {
            g_AppState.isDarkTheme = false;
            ImGui::StyleColorsLight();
        }

        ImGui::Separator();
        if (ImGui::Button(u8"自定义颜色..."))
        {
            ImGui::OpenPopup(u8"自定义颜色");
        }

        if (ImGui::BeginPopupModal(u8"自定义颜色", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            static ImVec4 text_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            static ImVec4 bg_color = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
            static ImVec4 button_color = ImGui::GetStyleColorVec4(ImGuiCol_Button);

            ImGui::ColorEdit3(u8"文本颜色", (float*)&text_color);
            ImGui::ColorEdit3(u8"背景颜色", (float*)&bg_color);
            ImGui::ColorEdit3(u8"按钮颜色", (float*)&button_color);

            if (ImGui::Button(u8"应用", ImVec2(120, 0)))
            {
                ImGui::GetStyle().Colors[ImGuiCol_Text] = text_color;
                ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = bg_color;
                ImGui::GetStyle().Colors[ImGuiCol_Button] = button_color;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"取消", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();
        if (ImGui::Button(u8"确定", ImVec2(120, 0)))
        {
            *p_open = false;
        }
    }
    ImGui::End();
}

// 帮助文档窗口实现
void ShowHelpWindow(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(u8"帮助文档", p_open))
    {
        ImGui::Text(u8"Demo GUI 应用程序使用指南");
        ImGui::Separator();

        ImGui::Text(u8"快捷键:");
        ImGui::BulletText(u8"Ctrl+N: 新建文件");
        ImGui::BulletText(u8"Ctrl+O: 打开文件");
        ImGui::BulletText(u8"Ctrl+S: 保存文件");
        ImGui::BulletText(u8"Alt+F4: 退出程序");

        ImGui::Separator();
        ImGui::Text(u8"基本操作:");
        ImGui::BulletText(u8"点击菜单栏中的「文件」可进行文件操作");
        ImGui::BulletText(u8"点击菜单栏中的「设置」可进行个性化设置");
        ImGui::BulletText(u8"点击菜单栏中的「帮助」可查看帮助文档和关于信息");

        ImGui::Separator();
        if (ImGui::Button(u8"关闭", ImVec2(120, 0)))
        {
            *p_open = false;
        }
    }
    ImGui::End();
}

// 检查更新窗口实现
void ShowUpdateCheckWindow(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(u8"检查更新", p_open, ImGuiWindowFlags_NoResize))
    {
        static bool checking = false;
        static bool finished = false;
        static bool hasUpdate = false;

        if (!checking && !finished)
        {
            if (ImGui::Button(u8"开始检查更新", ImVec2(150, 0)))
            {
                checking = true;
                // 在实际应用中，这里应该是一个异步操作
                // 演示目的，我们使用一个简单的模拟
                finished = false;
                hasUpdate = false;
            }
        }
        else if (checking)
        {
            ImGui::Text(u8"正在检查更新...");
            static float progress = 0.0f;
            progress += 0.01f;
            if (progress >= 1.0f)
            {
                progress = 0.0f;
                checking = false;
                finished = true;
                // 随机决定是否有更新（实际中这应该由真实的更新检查逻辑决定）
                hasUpdate = (rand() % 2) == 0;
            }
            ImGui::ProgressBar(progress);
        }
        else if (finished)
        {
            if (hasUpdate)
            {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), u8"发现新版本！");
                ImGui::Text(u8"新版本: 1.1.0");
                ImGui::Text(u8"当前版本: 1.0.0");

                if (ImGui::Button(u8"下载更新", ImVec2(120, 0)))
                {
                    // 实际应用中应该跳转到下载页面或者开始下载
                    ImGui::OpenPopup(u8"开始下载");
                }
                ImGui::SameLine();
                if (ImGui::Button(u8"稍后提醒", ImVec2(120, 0)))
                {
                    *p_open = false;
                }

                if (ImGui::BeginPopupModal(u8"开始下载", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text(u8"正在准备下载更新...");
                    static float download_progress = 0.0f;
                    download_progress += 0.005f;
                    if (download_progress < 1.0f)
                    {
                        ImGui::ProgressBar(download_progress);
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), u8"下载完成！");
                        if (ImGui::Button(u8"安装", ImVec2(120, 0)))
                        {
                            ImGui::CloseCurrentPopup();
                            *p_open = false;
                        }
                    }
                    ImGui::EndPopup();
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), u8"您的应用程序已是最新版本！");
                ImGui::Text(u8"当前版本: 1.0.0");

                if (ImGui::Button(u8"确定", ImVec2(120, 0)))
                {
                    *p_open = false;
                }
            }
        }
    }
    ImGui::End();
}

// 文件操作函数实现
bool SaveFile(const std::string& filePath, const std::string& content)
{
    std::ofstream file(filePath);
    if (!file.is_open())
    {
        return false;
    }

    file << content;
    file.close();
    return true;
}

bool LoadFile(const std::string& filePath, std::string& content)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        return false;
    }

    content.clear();
    std::string line;
    while (std::getline(file, line))
    {
        content += line + "\n";
    }
    file.close();
    return true;
}


void ConstrainWindowsToMainViewport()
{
    // 获取主视口(应用程序窗口)的大小和位置
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 viewport_min = viewport->Pos;
    ImVec2 viewport_max = ImVec2(viewport->Pos.x + viewport->Size.x,
        viewport->Pos.y + viewport->Size.y);

    // 获取所有活动窗口
    ImGuiContext& g = *ImGui::GetCurrentContext();
    for (int i = 0; i < g.Windows.Size; i++)
    {
        ImGuiWindow* window = g.Windows[i];

        // 跳过不可见窗口、特殊窗口等
        if (!window->Active || window->Hidden || window->SkipItems ||
            (window->Flags & ImGuiWindowFlags_ChildWindow) ||
            (window->Flags & ImGuiWindowFlags_Tooltip) ||
            (window->Flags & ImGuiWindowFlags_Popup) ||
            (window->Flags & ImGuiWindowFlags_NoMove))
            continue;

        // 获取窗口尺寸和位置
        ImVec2 window_pos = window->Pos;
        ImVec2 window_size = window->Size;

        // 确定窗口的最小可见区域 - 至少保证标题栏可见
        float min_visible = window->TitleBarHeight > 0.0f ?
            window->TitleBarHeight : 30.0f;

        // 约束窗口位置
        bool position_changed = false;

        // X轴约束 - 确保窗口不会完全超出左侧或右侧
        if (window_pos.x < viewport_min.x)
        {
            window_pos.x = viewport_min.x;
            position_changed = true;
        }
        if (window_pos.x + window_size.x > viewport_max.x)
        {
            window_pos.x = viewport_max.x - window_size.x;
            position_changed = true;
        }

        // Y轴约束 - 确保窗口不会完全超出顶部或底部
        if (window_pos.y < viewport_min.y)
        {
            window_pos.y = viewport_min.y;
            position_changed = true;
        }
        if (window_pos.y + window_size.y > viewport_max.y)
        {
            window_pos.y = viewport_max.y - window_size.y;
            position_changed = true;
        }

        // 如果位置需要调整，设置新位置
        if (position_changed)
        {
            ImGui::SetWindowPos(window->Name, window_pos);
        }
    }
}

// 获取文件的最后修改时间
std::time_t GetFileLastModifiedTime(const std::string& filePath)
{
    struct stat result;
    if (stat(filePath.c_str(), &result) == 0)
        return result.st_mtime;
    return 0;
}

// 实现函数
// 修改LoadLogFileIncremental函数，强化文件读取逻辑
bool LoadLogFileIncremental(const std::string& filePath, std::string& content, std::streampos& lastPos)
{
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
    if (fileSize == 0)
    {
        content.clear();
        lastPos = 0;
        file.close();
        return true;
    }

    // 检查文件是否被截断或覆盖，或是否强制读取整个文件
    bool fileReset = false;
    if (fileSize < lastPos || fileModified || lastPos == 0)
    {
        // 若文件大小减小或修改时间变化，或lastPos是0，从头读取
        lastPos = 0;
        content.clear();
        fileReset = true;
    }

    // 如果没有新内容且没有要求重置，直接返回
    if (lastPos == fileSize && !fileReset)
    {
        file.close();
        return true;
    }

    // 定位到上次读取的位置
    file.seekg(lastPos);

    // 使用缓冲区读取，避免逐行读取引起的空行问题
    std::vector<char> buffer(4096);
    std::string newContent;

    while (!file.eof())
    {
        file.read(&buffer[0], buffer.size());
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0)
            newContent.append(&buffer[0], bytesRead);
    }

    // 更新最后读取位置
    lastPos = fileSize;

    // 添加到已有内容
    content += newContent;

    file.close();
    return true;
}

void OpenLogFile()
{
    OPENFILENAMEA ofn;
    char szFile[260] = { 0 };

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "日志文件\0*.log;*.txt\0所有文件\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE)
    {
        std::string newFilePath = ofn.lpstrFile;

        // 无论是否选择了相同的文件，都重置状态
        g_AppState.logFilePath = newFilePath;
        g_AppState.logContent.clear();
        g_AppState.lastReadPosition = 0;
        g_AppState.logFileLastModified = 0; // 重置修改时间
        g_AppState.logContentCleared = false; // 重置清空标志

        // 获取文件修改时间并读取内容
        g_AppState.logFileLastModified = GetFileLastModifiedTime(g_AppState.logFilePath);
        LoadLogFileIncremental(g_AppState.logFilePath, g_AppState.logContent, g_AppState.lastReadPosition);
    }
}

void ClearLogContent()
{
    g_AppState.logContent.clear();
    g_AppState.logContentCleared = true; // 设置清空标志
    g_AppState.lastReadPosition = 0;     // 重置读取位置，确保下次刷新时从头读取
}

// 用于解析日志内容，生成带有行号和换行的文本
void RenderLogContent(const std::string& content, bool showLineNumbers, bool wrapText, float wrapWidth)
{
    if (content.empty())
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), u8"暂无日志内容");
        return;
    }

    // 计算实际的换行宽度，如果是0则使用窗口内容区域宽度
    float effectiveWrapWidth = wrapWidth;
    if (effectiveWrapWidth <= 0.0f)
    {
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
    if (showLineNumbers)
    {
        // 使用缩小的列宽来显示行号
        ImGui::BeginTable("LogContentTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit);
        ImGui::TableSetupColumn("LineNumbers", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);
    }

    while (std::getline(stream, line))
    {
        if (showLineNumbers)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%d", lineNumber);
            ImGui::TableSetColumnIndex(1);

            if (wrapText)
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + effectiveWrapWidth);
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopTextWrapPos();
            }
            else
            {
                ImGui::TextUnformatted(line.c_str());
            }
        }
        else
        {
            // 不显示行号时直接渲染内容
            if (wrapText)
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + effectiveWrapWidth);
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopTextWrapPos();
            }
            else
            {
                ImGui::TextUnformatted(line.c_str());
            }
        }

        lineNumber++;
    }

    if (showLineNumbers)
    {
        ImGui::EndTable();
    }
}
// 修改ShowLogWindow函数
void ShowLogWindow(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(u8"日志窗口", p_open))
    {
        // 文件控制栏
        if (ImGui::Button(u8"打开日志文件"))
        {
            OpenLogFile();
            g_AppState.logContentCleared = false; // 重置清空标志
        }

        ImGui::SameLine();
        if (ImGui::Button(u8"清空显示"))
        {
            ClearLogContent();
        }

        ImGui::SameLine();
        bool prevAutoRefresh = g_AppState.autoRefreshLog;
        ImGui::Checkbox(u8"自动刷新", &g_AppState.autoRefreshLog);

        // 如果手动切换自动刷新状态，重置清空标志并立即刷新
        if (prevAutoRefresh != g_AppState.autoRefreshLog)
        {
            g_AppState.logContentCleared = false;
            if (g_AppState.autoRefreshLog && !g_AppState.logFilePath.empty())
            {
                LoadLogFileIncremental(g_AppState.logFilePath, g_AppState.logContent, g_AppState.lastReadPosition);
                g_AppState.lastRefreshTime = ImGui::GetTime();
            }
        }

        if (g_AppState.autoRefreshLog)
        {
            ImGui::SameLine();
            ImGui::PushItemWidth(100.0f);
            ImGui::SliderFloat(u8"刷新间隔(秒)", &g_AppState.refreshInterval, 0.1f, 5.0f);
            ImGui::PopItemWidth();

            // 检查是否应该刷新 - 即使在logContentCleared=true的情况下也尝试刷新
            float currentTime = ImGui::GetTime();
            if (currentTime - g_AppState.lastRefreshTime >= g_AppState.refreshInterval)
            {
                if (!g_AppState.logFilePath.empty())
                {
                    // 如果已清空，需要重置cleared标志并重新读取整个文件
                    if (g_AppState.logContentCleared)
                    {
                        g_AppState.lastReadPosition = 0;  // 从头开始读取
                        g_AppState.logContent.clear();
                        g_AppState.logContentCleared = false;
                    }

                    LoadLogFileIncremental(g_AppState.logFilePath, g_AppState.logContent, g_AppState.lastReadPosition);
                }
                g_AppState.lastRefreshTime = currentTime;
            }
        }
        else
        {
            ImGui::SameLine();
            if (ImGui::Button(u8"刷新"))
            {
                if (!g_AppState.logFilePath.empty())
                {
                    // 如果已清空，需要重置cleared标志并重新读取整个文件
                    if (g_AppState.logContentCleared)
                    {
                        g_AppState.lastReadPosition = 0;  // 从头开始读取
                        g_AppState.logContent.clear();
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

        if (g_AppState.wrapLogLines)
        {
            ImGui::SameLine();
            ImGui::PushItemWidth(100.0f);
            ImGui::SliderFloat(u8"行宽", &g_AppState.logLineWidth, 0.0f, 1000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(u8"0 = 自适应窗口宽度");
            ImGui::PopItemWidth();
        }

        ImGui::Separator();

        // 显示当前日志文件信息
        if (!g_AppState.logFilePath.empty())
        {
            // 获取文件信息
            struct stat fileInfo;
            bool fileExists = (stat(g_AppState.logFilePath.c_str(), &fileInfo) == 0);

            if (fileExists)
            {
                // 转换文件大小为可读格式
                std::string sizeStr;
                if (fileInfo.st_size < 1024)
                    sizeStr = std::to_string(fileInfo.st_size) + " bytes";
                else if (fileInfo.st_size < 1024 * 1024)
                    sizeStr = std::to_string(fileInfo.st_size / 1024) + " KB";
                else
                    sizeStr = std::to_string(fileInfo.st_size / (1024 * 1024)) + " MB";

                // 转换时间为可读格式
                char timeBuffer[64];
                std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S",
                    std::localtime(&fileInfo.st_mtime));

                ImGui::TextWrapped(u8"当前日志文件: %s", g_AppState.logFilePath.c_str());
                ImGui::TextWrapped(u8"文件大小: %s | 最后修改: %s", sizeStr.c_str(), timeBuffer);
                ImGui::TextWrapped(u8"已读取位置: %lld / %lld", (long long)g_AppState.lastReadPosition, (long long)fileInfo.st_size);

                // 添加清空状态显示
                if (g_AppState.logContentCleared)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), u8"日志显示已清空，点击\"刷新\"重新加载");
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), u8"文件不存在: %s", g_AppState.logFilePath.c_str());
            }
        }
        else
        {
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
    ImGui::End();
}












//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
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

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
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
