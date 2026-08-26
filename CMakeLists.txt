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
    # 自定义 gtest 测试发现：系统 gtest 1.17 的 JSON 输出会损坏非 ASCII 测试名，
    # 导致 gtest_discover_tests 生成的双编码过滤器匹配不到任何中文名用例
    # （空跑通过）。改用 stdout 解析，见 cmake/DiscoverGTestTests.cmake。
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
endif()

add_subdirectory(server)
add_subdirectory(client)
