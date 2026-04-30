cmake_minimum_required(VERSION 4.1.1)
project(server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PostgreSQL REQUIRED)

# 添加 OpenSSL 依赖，jwt-cpp 需要用到其加密库
find_package(OpenSSL REQUIRED)

# 使用 pkg-config 查找 libpqxx
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

add_executable(server
    main.cpp
    DBConnection.cpp DBConnection.h
    Router.h Router.cpp
    auth_middleware.h auth_middleware.cpp
    ../contracts/include/gocook/IServices.h
    services/RecipeServiceImpl.cpp
    services/RecipeServiceImpl.h
    handlers/InventoryHandler.cpp handlers/InventoryHandler.h handlers/RecipeHandler.cpp handlers/RecipeHandler.h handlers/UserHandler.cpp handlers/UserHandler.h
    services/UserServiceImpl.h services/UserServiceImpl.cpp
    services/InventoryServiceImpl.h services/InventoryServiceImpl.cpp
    ../third_party/httplib/httplib.h
    ../third_party/nlohmann/json.hpp
    handlers/MealPlanHandler.h handlers/MealPlanHandler.cpp
    handlers/AnnouncementHandler.h handlers/AnnouncementHandler.cpp
    handlers/AdminHandler.h handlers/AdminHandler.cpp
    services/MealPlanServiceImpl.h services/MealPlanServiceImpl.cpp
    services/AnnouncementServiceImpl.h services/AnnouncementServiceImpl.cpp
    services/AdminServiceImpl.h services/AdminServiceImpl.cpp
    ../third_party/jwt-cpp/jwt.h
    ../third_party/picojson/picojson.h
    ../third_party/jwt-cpp/base.h
    ../third_party/jwt-cpp/traits/kazuho-picojson/defaults.h ../third_party/jwt-cpp/traits/kazuho-picojson/traits.h
)

# 增加头文件查找路径
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/../contracts/include)
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/../third_party)
target_include_directories(server PRIVATE ${PQXX_INCLUDE_DIRS})
target_include_directories(server PRIVATE ${OPENSSL_INCLUDE_DIR})

target_link_directories(server PRIVATE ${PQXX_LIBRARY_DIRS})

target_link_libraries(server ${PQXX_LIBRARIES} PostgreSQL::PostgreSQL OpenSSL::SSL OpenSSL::Crypto)

target_compile_features(server PRIVATE cxx_std_23)
