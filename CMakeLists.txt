cmake_minimum_required(VERSION 3.10)
project(SU2GUI)

# 设置 Visual Studio 默认启动项目
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SU2GUI)

# 设置默认构建类型为 Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# 设置编译优化选项
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# 设置统一输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# 头文件路径
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/packages/win32_directx11/include)
include_directories(${CMAKE_SOURCE_DIR}/packages/misc/freetype)
include_directories(${CMAKE_SOURCE_DIR}/packages/freetype-2.13.3/include)
include_directories(${CMAKE_SOURCE_DIR}/packages/implot/include)  # 添加ImPlot头文件路径
include_directories(${CMAKE_SOURCE_DIR}/packages/implot3d/include)  # 添加ImPlot3d头文件路径

# 搜索源文件
file(GLOB SOURCES
    "${CMAKE_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_SOURCE_DIR}/packages/win32_directx11/source/*.cpp"
    "${CMAKE_SOURCE_DIR}/packages/misc/freetype/*.cpp"
    "${CMAKE_SOURCE_DIR}/packages/implot/source/*.cpp"  # 添加ImPlot源文件
    "${CMAKE_SOURCE_DIR}/packages/implot3d/source/*.cpp"  # 添加ImPlot3d源文件
)

# 库文件路径
link_directories(${CMAKE_SOURCE_DIR}/packages/win32_directx11/lib/x64)
link_directories(${CMAKE_SOURCE_DIR}/packages/freetype-2.13.3/lib)

# 创建可执行文件 - 将普通可执行文件改为WIN32应用程序
add_executable(SU2GUI WIN32 ${SOURCES} ${HEADERS})

# 根据编译器类型设置链接库
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # GCC 编译器
    target_link_libraries(SU2GUI PRIVATE libd3d11.a libd3dcompiler.a libdxgi.a freetype dwmapi)
else()
    # MSVC 编译器或其他
    target_link_libraries(SU2GUI PRIVATE d3d11.lib d3dcompiler.lib dxgi.lib freetype.lib dwmapi.lib)
endif()

# 组织文件到 VS 解决方案中的实际目录结构
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

# 根据 Debug/Release 配置修改输出文件名
set_target_properties(SU2GUI PROPERTIES
    OUTPUT_NAME "SU2GUI"          # 基本名称
    DEBUG_POSTFIX "_Debug"         # Debug 模式下添加后缀
    RELEASE_POSTFIX ""             # Release 模式下无后缀
)

# 根据编译器类型设置链接库
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # 添加构建后命令，复制 freetype.dll 到构建目录
    add_custom_command(TARGET SU2GUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/packages/freetype-2.13.3/lib/libfreetype.dll"
        "$<TARGET_FILE_DIR:SU2GUI>")
endif()