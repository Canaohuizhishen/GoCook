set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Sql Core Network Qml)
find_package(GTest REQUIRED)

set(TEST_SOURCES
    test_main.cpp
    test_local_database.cpp
    test_http_api.cpp
    test_inventory_vm.cpp
    test_recipe_vm.cpp
)

add_executable(test_client ${TEST_SOURCES}
    ../database/LocalDatabase.h ../database/LocalDatabase.cpp
    ../api/HttpGoCookApi.h ../api/HttpGoCookApi.cpp
    ../adapters/DataMapper.h ../adapters/DataMapper.cpp
    ../viewmodels/InventoryViewModel.h ../viewmodels/InventoryViewModel.cpp
    ../viewmodels/RecipeViewModel.h ../viewmodels/RecipeViewModel.cpp
)

target_include_directories(test_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../database
    ${CMAKE_CURRENT_SOURCE_DIR}/../api
    ${CMAKE_CURRENT_SOURCE_DIR}/../adapters
    ${CMAKE_CURRENT_SOURCE_DIR}/../viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/../../contracts/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party
)

target_link_libraries(test_client PRIVATE
    Qt6::Sql
    Qt6::Core
    Qt6::Network
    Qt6::Qml
    GTest::gtest
)

target_compile_features(test_client PUBLIC cxx_std_23)

# 自定义测试发现：系统 gtest 1.17 的 JSON 输出会损坏非 ASCII 测试名，
# 标准 gtest_discover_tests 生成的过滤器匹配不到中文名用例（ctest 空跑通过）
include(DiscoverGTestTests)
gocook_discover_gtest_tests(test_client)
