include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

set(TEST_SOURCES
    test_user_service.cpp
    test_recipe_service.cpp
    test_inventory_service.cpp
    test_unimplemented_services.cpp
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

gtest_discover_tests(test_services)
