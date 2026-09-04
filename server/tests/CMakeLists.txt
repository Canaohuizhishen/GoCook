find_package(GTest REQUIRED)

# 自定义测试发现：系统 gtest 1.17 的 JSON 输出会损坏非 ASCII 测试名，
# 标准 gtest_discover_tests 生成的过滤器匹配不到中文名用例（ctest 空跑通过）
include(DiscoverGTestTests)

set(TEST_SOURCES
    test_user_service.cpp
    test_recipe_service.cpp
    test_recipe_nutrition.cpp
    test_inventory_service.cpp
    test_inventory_integration.cpp
    test_unimplemented_services.cpp
    test_db_executor.cpp
    test_upload_paths.cpp
    test_nutrition_integration.cpp
)

add_executable(test_services ${TEST_SOURCES})

target_include_directories(test_services PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(test_services PRIVATE
    gocook-server-lib
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
)

target_compile_features(test_services PUBLIC cxx_std_23)

gocook_discover_gtest_tests(test_services)
