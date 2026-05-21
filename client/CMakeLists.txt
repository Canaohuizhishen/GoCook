cmake_minimum_required(VERSION 4.1.1)

#https://github.com/Kitware/CMake/blob/v4.1.1/Help/dev/experimental.rst

project(client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Sql Qml Core5Compat)

# 自动设置所有推荐的策略
qt_standard_project_setup()

qt_add_executable(appclient)

qt_add_qml_module(appclient
    URI client
    VERSION 1.0
    SOURCES main.cpp
    SOURCES api/HttpGoCookApi.h api/HttpGoCookApi.cpp
    SOURCES database/LocalDatabase.h database/LocalDatabase.cpp
    QML_FILES qml/pages/LoginPage.qml
    QML_FILES qml/pages/HomePage.qml
    QML_FILES qml/pages/RecommendPage.qml
    QML_FILES qml/pages/InventoryPage.qml
    QML_FILES qml/pages/ProfilePage.qml
    QML_FILES qml/pages/RecipeDetailPage.qml
    QML_FILES qml/pages/NutritionReportPage.qml
    QML_FILES qml/pages/SubmitRecipePage.qml
    QML_FILES qml/Main.qml
    QML_FILES qml/styles/Theme.qml
    QML_FILES qml/components/CustomButton.qml
    QML_FILES qml/components/RecipeCard.qml
    QML_FILES qml/components/RecipeGridCard.qml
    QML_FILES qml/components/LoadingIndicator.qml
    QML_FILES qml/pages/FavoritesPage.qml
    SOURCES ../contracts/include/gocook/DataModels.h
    SOURCES ../contracts/include/gocook/IGoCookApi.h
    SOURCES viewmodels/RecipeViewModel.cpp viewmodels/RecipeViewModel.h
    SOURCES viewmodels/AuthViewModel.cpp viewmodels/AuthViewModel.h
    SOURCES viewmodels/InventoryViewModel.cpp viewmodels/InventoryViewModel.h
    SOURCES adapters/DataMapper.h adapters/DataMapper.cpp
    SOURCES viewmodels/InventoryViewModel.cpp viewmodels/InventoryViewModel.h
    QML_FILES qml/pages/RecipeDetailPage.qml qml/pages/SubmitRecipePage.qml
    QML_FILES qml/pages/SearchPage.qml
    QML_FILES qml/pages/RecommendResultsPage.qml
    QML_FILES qml/pages/MyRecipesPage.qml
    QML_FILES qml/pages/MyRatingsPage.qml
)

# 增加头文件查找路径
target_include_directories(appclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../contracts/include)
target_include_directories(appclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)
target_include_directories(appclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/adapters)

target_sources(appclient
    PRIVATE
        #math.cpp                   # 实现单元
    PRIVATE FILE_SET cxx_modules TYPE CXX_MODULES  FILES
        #math.cppm                  # 接口单元
)

source_group("QML Files" FILES
    Main.qml
)

target_compile_features(appclient PRIVATE cxx_std_23)

target_link_libraries(appclient
    PRIVATE
        Qt6::Quick
        Qt6::Sql
        Qt6::Qml
        Qt6::Core5Compat
)

set_target_properties(appclient PROPERTIES
    AUTORCC TRUE                              #  set(CMAKE_AUTORCC ON)
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appclient
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS appclient
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)


















































# set(CMAKE_CXX_STANDARD_REQUIRED ON) # Globally preventing the fallback behaviour of C++ standard
#qt_add_executable(client)  //Creates and finalizes an application target of a platform-specific type.
#This command performs the following tasks:
#    1. Create a CMake target of the appropriate type for the target platform.
#    2. Link the target to the Qt::Core library.
#    3. Handle finalization of the CMake target.

# set(CMAKE_CXX_STANDARD_REQUIRED ON) # Globally preventing the fallback behaviour of C++ standard

# qt_standard_project_setup()  #set(CMAKE_AUTOUIC ON)  set(CMAKE_AUTOMOC ON)






# cmake_minimum_required(VERSION 3.16)
#
# project(client VERSION 0.1 LANGUAGES CXX)
#
# @if !false
# set(CMAKE_AUTOMOC ON)
# @endif
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
#
# find_package(Qt6 REQUIRED COMPONENTS Quick)
#
# @if false
# @if false
# qt_standard_project_setup(REQUIRES 6.10)
# @else
# qt_standard_project_setup()
# @endif
# @endif
#
# qt_add_executable(appclient
#     main.cpp
# )
#
# qt_add_qml_module(appclient
#     URI client
#     VERSION 1.0
#     QML_FILES
#         Main.qml
# )
#
# # Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# # If you are developing for iOS or macOS you should consider setting an
# # explicit, fixed bundle identifier manually though.
# set_target_properties(appclient PROPERTIES
# #    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appclient
#     MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
#     MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
#     MACOSX_BUNDLE TRUE
#     WIN32_EXECUTABLE TRUE
# )
#
# target_link_libraries(appclient
#     PRIVATE Qt6::Quick
# )
#
# include(GNUInstallDirs)
# install(TARGETS appclient
#     BUNDLE DESTINATION .
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
# )
