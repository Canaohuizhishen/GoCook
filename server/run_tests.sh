#!/usr/bin/env bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build}"

echo "=== 配置构建 ==="
# 单独 configure 本目录时需显式注入仓库根 cmake/ 模块路径：
# tests/CMakeLists.txt 的 include(DiscoverGTestTests) 只被顶层 CMakeLists 默认追加过
# CMAKE_MODULE_PATH，单目录 configure（本脚本 / Dockerfile）不带上会 include 失败。
cmake -B "$BUILD_DIR" -G Ninja -S "$SCRIPT_DIR" \
    -DCMAKE_MODULE_PATH="$SCRIPT_DIR/../cmake" -DBUILD_TESTING=ON

echo ""
echo "=== 编译测试 ==="
cmake --build "$BUILD_DIR" --target test_services

echo ""
echo "=== 运行测试 ==="
echo ""
"$BUILD_DIR/tests/test_services" --gtest_print_time=0 2>&1 | sed \
    -e 's/\[==========\] Running \([0-9]*\) tests from \([0-9]*\) test suites/========== 运行 \1 个测试，来自 \2 个测试套件/' \
    -e 's/\[----------\] Global test environment set-up/---------- 全局测试环境初始化/' \
    -e 's/\[----------\] Global test environment tear-down/---------- 全局测试环境清理/' \
    -e 's/\[----------\] [0-9]* test[s]* from/-------- 测试来自/' \
    -e '/\[ RUN      \]/d' \
    -e 's/\[       OK \]/✅ /' \
    -e 's/\[  FAILED  \]/[   失败   ]/' \
    -e 's/\[  PASSED  \]/[    通过 ]/' \
    -e 's/Global test environment set-up/全局测试环境初始化/' \
    -e 's/Global test environment tear-down/全局测试环境清理/' \
    -e 's/\[==========\] [0-9]* tests from [0-9]* test suites ran\.$/========================================================/' \
    -e 's/\[    通过 \] \([0-9]*\) tests\./🎉 全部通过！\1 个测试/'
