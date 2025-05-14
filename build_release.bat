@echo off
chcp 65001
cmake --build .\build\ --config Release --target Imgui -j 18 --