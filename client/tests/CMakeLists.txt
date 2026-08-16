set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Sql Core Network Qml)
find_package(GTest REQUIRED)

set(TEST_SOURCES
    test_main.cpp
    test_local_database.cpp
    test_http_api.cpp
)

add_executable(test_client ${TEST_SOURCES}
    ../database/LocalDatabase.h ../database/LocalDatabase.cpp
    ../api/HttpGoCookApi.h ../api/HttpGoCookApi.cpp
)

target_include_directories(test_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../database
    ${CMAKE_CURRENT_SOURCE_DIR}/../api
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

include(GoogleTest)
gtest_discover_tests(test_client)
