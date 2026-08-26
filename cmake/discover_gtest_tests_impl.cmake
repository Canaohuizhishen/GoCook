# ────────────────────────────────────────────────────────────────
# 发现实现（cmake -P 脚本模式，POST_BUILD 调用）
#
# 输入参数：
#   TEST_EXECUTABLE  测试二进制绝对路径
#   TEST_WORKING_DIR 发现时的工作目录（与被测运行时一致）
#   TEST_OUTPUT_FILE 生成的 add_test 列表文件（写满后由 ctest include）
#
# 解析 --gtest_list_tests 的 stdout（原始 UTF-8）：
#   套件行以 '.' 结尾且不缩进；用例行以两个空格缩进。
#   生成 add_test([==[套件.用例]==] <二进制> [==[--gtest_filter=套件.用例]==] ...)
# ────────────────────────────────────────────────────────────────

foreach(v TEST_EXECUTABLE TEST_WORKING_DIR TEST_OUTPUT_FILE)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "discover_gtest_tests_impl: ${v} 未指定")
  endif()
endforeach()

file(REMOVE "${TEST_OUTPUT_FILE}")

execute_process(
  COMMAND "${TEST_EXECUTABLE}" --gtest_list_tests
  WORKING_DIRECTORY "${TEST_WORKING_DIR}"
  OUTPUT_VARIABLE output
  RESULT_VARIABLE result)

if(NOT result EQUAL 0)
  string(REPLACE "\n" "\n    " output "${output}")
  message(FATAL_ERROR
    "discover_gtest_tests_impl: 运行 ${TEST_EXECUTABLE} --gtest_list_tests 失败（${result}）：\n    ${output}")
endif()

# 按行拆分；先转义分号防止测试名中的分号破坏 list 语义
string(REPLACE [[;]] [[\;]] output "${output}")
string(REPLACE "\n" ";" output "${output}")

set(current_suite "")
set(count 0)
foreach(line IN LISTS output)
  # gtest 启动横幅（"Running main() from ...gtest_main.cc"）
  if(line MATCHES "gtest_main\\.cc")
    continue()
  endif()
  if(line STREQUAL "")
    continue()
  endif()

  if(NOT line MATCHES "^  ")
    # 套件行：去掉结尾 '.' 与行内注释（如 "# TypeParam = ..."）
    string(REGEX REPLACE "\\.( *#.*)?$" "" current_suite "${line}")
  else()
    # 用例行：去掉缩进与行内注释（如 "# GetParam() = ..."）
    string(STRIP "${line}" test)
    string(REGEX REPLACE " ( *#.*)?$" "" current_test "${test}")
    if(NOT current_suite STREQUAL "" AND NOT current_test STREQUAL "")
      file(APPEND "${TEST_OUTPUT_FILE}"
        "add_test([==[${current_suite}.${current_test}]==]"
        " \"${TEST_EXECUTABLE}\""
        " [==[--gtest_filter=${current_suite}.${current_test}]==]"
        " --gtest_also_run_disabled_tests)\n")
      math(EXPR count "${count} + 1")
    endif()
  endif()
endforeach()

message(STATUS "discover_gtest_tests_impl: ${TEST_EXECUTABLE} -> ${count} 个用例 -> ${TEST_OUTPUT_FILE}")
