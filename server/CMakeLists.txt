cmake_minimum_required(VERSION 4.1.1)
project(server LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PostgreSQL REQUIRED)

# 添加 OpenSSL 依赖，jwt-cpp 需要用到其加密库
find_package(OpenSSL REQUIRED)

# 使用 pkg-config 查找 libpqxx
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

# ---- 可复用的服务器库（不含 main.cpp）----
set(SERVER_LIB_SOURCES
    common/ConnectionPool.cpp common/ConnectionPool.h
    Router.h Router.cpp
    middleware/auth_middleware.h middleware/auth_middleware.cpp
    middleware/RateLimiter.h middleware/RateLimiter.cpp
    ../contracts/include/gocook/IServices.h
    ../contracts/include/gocook/DataModels.h
    ../contracts/include/gocook/IUserRepository.h
    ../contracts/include/gocook/IRecipeRepository.h
    ../contracts/include/gocook/IInventoryRepository.h
    ../contracts/include/gocook/IMealPlanRepository.h
    ../contracts/include/gocook/IAnnouncementRepository.h
    ../contracts/include/gocook/IAdminRepository.h
    services/RecipeServiceImpl.cpp services/RecipeServiceImpl.h
    services/UserServiceImpl.h services/UserServiceImpl.cpp
    services/InventoryServiceImpl.h services/InventoryServiceImpl.cpp
    services/MealPlanServiceImpl.h services/MealPlanServiceImpl.cpp
    services/AnnouncementServiceImpl.h services/AnnouncementServiceImpl.cpp
    services/AdminServiceImpl.h services/AdminServiceImpl.cpp
    repositories/PgUserRepository.h repositories/PgUserRepository.cpp
    repositories/PgRecipeRepository.h repositories/PgRecipeRepository.cpp
    repositories/PgInventoryRepository.h repositories/PgInventoryRepository.cpp
    repositories/PgMealPlanRepository.h repositories/PgMealPlanRepository.cpp
    repositories/PgAnnouncementRepository.h repositories/PgAnnouncementRepository.cpp
    repositories/PgAdminRepository.h repositories/PgAdminRepository.cpp
    handlers/RecipeHandler.h handlers/RecipeHandler.cpp
    handlers/UserHandler.h handlers/UserHandler.cpp
    handlers/InventoryHandler.h handlers/InventoryHandler.cpp
    handlers/MealPlanHandler.h handlers/MealPlanHandler.cpp
    handlers/AnnouncementHandler.h handlers/AnnouncementHandler.cpp
    handlers/AdminHandler.h handlers/AdminHandler.cpp
    common/ErrorHelper.h
    common/PaginationHelper.h
    common/AuthHelper.h
    common/SerializationHelper.h
    common/JsonSerializer.h common/JsonSerializer.cpp
    common/Validation.h
    common/Config.h
    common/Logger.h
    ../third_party/httplib/httplib.h
    ../third_party/nlohmann/json.hpp
    ../third_party/jwt-cpp/jwt.h
    ../third_party/picojson/picojson.h
    ../third_party/jwt-cpp/base.h
    ../third_party/jwt-cpp/traits/kazuho-picojson/defaults.h
    ../third_party/jwt-cpp/traits/kazuho-picojson/traits.h
    ../third_party/bcrypt/crypt_blowfish.c ../third_party/bcrypt/crypt_blowfish.h
)

# 头文件查找路径
set(CONTRACTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../contracts/include")
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

add_library(gocook-server-lib STATIC ${SERVER_LIB_SOURCES}
    tests/CMakeLists.txt tests/MockInventoryRepository.h tests/MockRecipeRepository.h tests/MockUserRepository.h tests/test_inventory_service.cpp tests/test_recipe_service.cpp tests/test_unimplemented_services.cpp tests/test_user_service.cpp)

target_include_directories(gocook-server-lib PUBLIC
    ${CONTRACTS_DIR}
    ${THIRD_PARTY_DIR}
)
target_include_directories(gocook-server-lib PRIVATE
    ${PQXX_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_directories(gocook-server-lib PRIVATE ${PQXX_LIBRARY_DIRS})

target_link_libraries(gocook-server-lib PUBLIC
    ${PQXX_LIBRARIES}
    PostgreSQL::PostgreSQL
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_compile_features(gocook-server-lib PUBLIC cxx_std_23)

# ---- 服务端主程序 ----
add_executable(server main.cpp)
target_link_libraries(server PRIVATE gocook-server-lib)

# ---- 单元测试 ----
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
