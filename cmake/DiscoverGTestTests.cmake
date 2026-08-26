# ────────────────────────────────────────────────────────────────
# 自定义 gtest 测试发现（替代 gtest_discover_tests）
#
# 背景：系统 gtest 1.17.0 的 --gtest_output=json 把非 ASCII 测试名按
# "字节 → \u00XX" 转义（UTF-8 字节被当作 Latin-1 码点），CMake 反转义后
# 测试名被双重编码，ctest 拿到的 --gtest_filter 匹配不到任何中文名测试
# （"did not match any test"）→ 所有中文名测试在 ctest 下空跑通过，
# "146/146""159/159" 之类的通过数对它们没有意义。
#
# 本模块改走 --gtest_list_tests 的标准输出：gtest 对 stdout 输出原始
# UTF-8 字节，execute_process 原样保留，解析后为每个用例生成一条
# add_test，粒度与 gtest_discover_tests 一致，中文名用例真正可跑。
# ────────────────────────────────────────────────────────────────

if(COMMAND gocook_discover_gtest_tests)
  return()
endif()

function(gocook_discover_gtest_tests TARGET)
  set(ctest_tests_file   "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_ctest_tests.cmake")
  set(ctest_include_file "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_ctest_include.cmake")
  set(discover_script    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/discover_gtest_tests_impl.cmake")

  # 构建该目标后自动重跑发现（与 gtest_discover_tests 的 POST_BUILD 模式一致）
  add_custom_command(TARGET ${TARGET} POST_BUILD
    BYPRODUCTS "${ctest_tests_file}"
    COMMAND "${CMAKE_COMMAND}"
            -D "TEST_EXECUTABLE=$<TARGET_FILE:${TARGET}>"
            -D "TEST_WORKING_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            -D "TEST_OUTPUT_FILE=${ctest_tests_file}"
            -P "${discover_script}"
    COMMENT "Discovering GTest tests for ${TARGET} (custom stdout parser)"
    VERBATIM)

  # ctest 运行时 include 发现结果；目标未构建时给出占位测试
  file(WRITE "${ctest_include_file}"
    "if(EXISTS \"${ctest_tests_file}\")\n"
    "  include(\"${ctest_tests_file}\")\n"
    "else()\n"
    "  add_test(${TARGET}_NOT_BUILT ${TARGET}_NOT_BUILT)\n"
    "endif()\n")

  set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${ctest_include_file}")
endfunction()
