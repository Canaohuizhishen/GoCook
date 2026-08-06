set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Sql Core)
find_package(GTest REQUIRED)

set(TEST_SOURCES
    test_main.cpp
    test_local_database.cpp
)

add_executable(test_client ${TEST_SOURCES}
    ../database/LocalDatabase.h ../database/LocalDatabase.cpp
)

target_include_directories(test_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../database
    ${CMAKE_CURRENT_SOURCE_DIR}/../api
)

target_link_libraries(test_client PRIVATE
    Qt6::Sql
    Qt6::Core
    GTest::gtest
)

target_compile_features(test_client PUBLIC cxx_std_23)

include(GoogleTest)
gtest_discover_tests(test_client)
