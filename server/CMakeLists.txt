cmake_minimum_required(VERSION 4.1.1)
project(server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PostgreSQL REQUIRED)

# 使用 pkg-config 查找 libpqxx
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

add_executable(server
    main.cpp
    DBConnection.cpp DBConnection.h
    RecipeHandler.cpp RecipeHandler.h
    Router.h Router.cpp
    UserHandler.h UserHandler.cpp
    auth_utils.h auth_utils.cpp
    InventoryHandler.h InventoryHandler.cpp
)

target_include_directories(server PRIVATE ${PQXX_INCLUDE_DIRS})
target_link_directories(server PRIVATE ${PQXX_LIBRARY_DIRS})
target_link_libraries(server ${PQXX_LIBRARIES} PostgreSQL::PostgreSQL)

target_compile_features(server PRIVATE cxx_std_23)
