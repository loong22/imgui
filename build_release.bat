@echo off
chcp 65001 > nul

REM 检查 build 目录是否存在，不存在则创建
if not exist build (
    mkdir build
    echo 创建 build 目录
)

REM 进入 build 目录
cd build

REM 清理之前的缓存（如果存在生成器冲突）
if exist CMakeCache.txt (
    echo 清理旧的 CMake 缓存...
    del CMakeCache.txt
    rmdir /s /q CMakeFiles 2>nul
)

REM 运行 CMake 配置
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release .. 2>&1

REM 执行构建操作
cmake --build . --config Release --target Imgui -j 8 2>&1

echo 生成完成
