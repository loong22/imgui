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

#include "../include/plot.h"
#include "imgui.h"
#include "implot.h"
#include "implot3d.h"
#include <cmath>

// 图表窗口实现
void ShowPlotWindow(bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(u8"图表窗口", p_open)) {
        // 创建一些示例数据
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

        // 显示控制选项
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
        
        // 选择图表类型
        static int plot_type = 0;
        ImGui::RadioButton(u8"折线图", &plot_type, 0);
        ImGui::SameLine();
        ImGui::RadioButton(u8"散点图", &plot_type, 1);
        
        // 创建图表 - 使用新版的BeginPlot API
        if (ImPlot::BeginPlot(u8"我的图表")) {
            // 手动设置轴标签
            ImPlot::SetupAxes(u8"X轴", u8"Y轴");
            
            if (plot_type == 0) { // 折线图
                ImPlot::PlotLine(u8"正弦波", xs, ys1, 100);
                ImPlot::PlotLine(u8"余弦波", xs, ys2, 100);
            }
            else { // 散点图
                ImPlot::PlotScatter(u8"正弦波", xs, ys1, 100);
                ImPlot::PlotScatter(u8"余弦波", xs, ys2, 100);
            }
            ImPlot::EndPlot();
        }
        
        // 添加一些高级设置选项
        ImGui::Separator();
        
        static bool show_advanced = false;
        if (ImGui::Button(u8"高级设置")) {
            show_advanced = !show_advanced;
        }
        
        if (show_advanced) {
            ImGui::BeginChild("AdvancedSettings", ImVec2(0, 120), true);
            
            static float line_thickness = 2.0f;
            ImGui::SliderFloat(u8"线条粗细", &line_thickness, 1.0f, 5.0f);
            ImPlot::GetStyle().LineWeight = line_thickness;
            
            static float marker_size = 3.0f;
            ImGui::SliderFloat(u8"标记大小", &marker_size, 1.0f, 10.0f);
            ImPlot::GetStyle().MarkerSize = marker_size;
            
            static ImVec4 line_color1 = ImVec4(0.0f, 0.7f, 0.4f, 1.0f);
            static ImVec4 line_color2 = ImVec4(0.0f, 0.4f, 0.7f, 1.0f);
            
            ImGui::ColorEdit4(u8"正弦波颜色", (float*)&line_color1);
            ImGui::ColorEdit4(u8"余弦波颜色", (float*)&line_color2);
            
            // 应用颜色设置（实际上在真实应用中应该在绘图前设置）
            ImPlot::GetColormapColor(0) = line_color1;
            ImPlot::GetColormapColor(1) = line_color2;
            
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

// 3D图表窗口实现
void DemoMeshPlots(bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(u8"3D图表窗口", p_open)) {
        static int mesh_id = 0;
        ImGui::Combo("Mesh", &mesh_id, "Duck\0Sphere\0Cube\0\0");

        // Choose fill color
        static bool set_fill_color = true;
        static ImVec4 fill_color = ImVec4(0.8f, 0.8f, 0.2f, 0.6f);
        ImGui::Checkbox("Fill Color", &set_fill_color);
        if (set_fill_color) {
            ImGui::SameLine();
            ImGui::ColorEdit4("##MeshFillColor", (float*)&fill_color);
        }

        // Choose line color
        static bool set_line_color = true;
        static ImVec4 line_color = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
        ImGui::Checkbox("Line Color", &set_line_color);
        if (set_line_color) {
            ImGui::SameLine();
            ImGui::ColorEdit4("##MeshLineColor", (float*)&line_color);
        }

        // Choose marker color
        static bool set_marker_color = false;
        static ImVec4 marker_color = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
        ImGui::Checkbox("Marker Color", &set_marker_color);
        if (set_marker_color) {
            ImGui::SameLine();
            ImGui::ColorEdit4("##MeshMarkerColor", (float*)&marker_color);
        }

        if (ImPlot3D::BeginPlot("Mesh Plots")) {
            ImPlot3D::SetupAxesLimits(-1, 1, -1, 1, -1, 1);

            // Set colors
            if (set_fill_color)
                ImPlot3D::SetNextFillStyle(fill_color);
            else {
                // If not set as transparent, the fill color will be determined by the colormap
                ImPlot3D::SetNextFillStyle(ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            }
            if (set_line_color)
                ImPlot3D::SetNextLineStyle(line_color);
            if (set_marker_color)
                ImPlot3D::SetNextMarkerStyle(ImPlot3DMarker_Square, 3, marker_color, IMPLOT3D_AUTO, marker_color);

            // Plot mesh
            if (mesh_id == 0)
                ImPlot3D::PlotMesh("Duck", ImPlot3D::duck_vtx, ImPlot3D::duck_idx, ImPlot3D::DUCK_VTX_COUNT, ImPlot3D::DUCK_IDX_COUNT);
            else if (mesh_id == 1)
                ImPlot3D::PlotMesh("Sphere", ImPlot3D::sphere_vtx, ImPlot3D::sphere_idx, ImPlot3D::SPHERE_VTX_COUNT, ImPlot3D::SPHERE_IDX_COUNT);
            else if (mesh_id == 2)
                ImPlot3D::PlotMesh("Cube", ImPlot3D::cube_vtx, ImPlot3D::cube_idx, ImPlot3D::CUBE_VTX_COUNT, ImPlot3D::CUBE_IDX_COUNT);

            ImPlot3D::EndPlot();
        }
        
        // 添加一些附加控制选项
        ImGui::Separator();
        
        static bool auto_rotate = true;
        ImGui::Checkbox(u8"自动旋转", &auto_rotate);
        
        if (auto_rotate) {
            static float rotation_speed = 0.01f;
            ImGui::SliderFloat(u8"旋转速度", &rotation_speed, 0.001f, 0.05f);
            
            // 这里应该是调用ImPlot3D提供的旋转功能
            // 由于ImPlot3D库的详细实现未知，此处仅作示例
            // ImPlot3D::SetAutoRotation(auto_rotate, rotation_speed);
        }
        
        ImGui::Text(u8"操作说明:");
        ImGui::BulletText(u8"左键拖动: 旋转模型");
        ImGui::BulletText(u8"右键拖动: 平移视图");
        ImGui::BulletText(u8"滚轮: 缩放视图");
    }
    ImGui::End();
}