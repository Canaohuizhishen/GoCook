cmake_minimum_required(VERSION 4.1.1)

# https://github.com/Kitware/CMake/blob/v4.1.1/Help/dev/experimental.rst

project(GoCook VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# 统一构建入口
# 一命令构建整个项目：cmake -B build -G Ninja && cmake --build build
# 构建产物：server/server（服务端），client/appclient（客户端）
# ============================================================

# ---- 单元测试（顶层唯一开关：ctest 在 build/ 根目录直接可用）----
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
endif()

add_subdirectory(server)
add_subdirectory(client)
