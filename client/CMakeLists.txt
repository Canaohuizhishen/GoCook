cmake_minimum_required(VERSION 4.1.1)

#https://github.com/Kitware/CMake/blob/v4.1.1/Help/dev/experimental.rst

project(client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Sql Qml Core5Compat Widgets)

# 显式固定两个 QML 策略为 OLD（与当前默认行为一致；固定后 configure 会出现"OLD 将在未来
# Qt 大版本移除"的预期提示——升级时 configure 显式爆掉，优于运行期静默换行为）：
#   QTP0001（资源前缀，Qt 6.5+）：OLD = 模块资源挂在 /<URI>（:/client/...），main.cpp 的
#     qrc:/client/... 加载路径与 QML 模块 dir map 均依赖该行为；迁移 NEW（/qt/qml 前缀）
#     另立专项，届时同步改 main.cpp 路径。
#   QTP0004（额外目录 qmldir 生成，Qt 6.8+）：OLD = 不为模块内各含 QML 的子目录额外生成
#     qmldir 文件（维持现状）。与资源前缀无关，两者勿混为一谈。
qt_policy(SET QTP0001 OLD)
qt_policy(SET QTP0004 OLD)

# 自动设置所有推荐的策略
qt_standard_project_setup()

qt_add_executable(appclient)

# 模块内 QML 单例声明（Theme / PagePolicy）：写入生成 qmldir 的 singleton 条目，
# qmllint / qmlcachegen 才能静态解析单例成员（否则所有 Theme.* 访问会误报 missing-property）
set_source_files_properties(qml/styles/Theme.qml qml/PagePolicy.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appclient
    URI client
    VERSION 1.0
    SOURCES main.cpp
    SOURCES api/HttpGoCookApi.h api/HttpGoCookApi.cpp api/JsonDeserializer.h api/JsonDeserializer.cpp
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
    QML_FILES qml/PagePolicy.qml
    QML_FILES qml/styles/Theme.qml
    QML_FILES qml/components/CustomButton.qml
    QML_FILES qml/components/RecipeCard.qml
    QML_FILES qml/components/RecipeGridCard.qml
    QML_FILES qml/components/LoadingIndicator.qml
    QML_FILES qml/components/PullToRefresh.qml
    QML_FILES qml/components/CircularImage.qml
    QML_FILES qml/components/SwipeToDeleteItem.qml
    QML_FILES qml/pages/FavoritesPage.qml
    QML_FILES qml/pages/ShoppingListPage.qml
    QML_FILES qml/pages/ShoppingListDetailPage.qml
    SOURCES ../contracts/include/gocook/DataModels.h
    SOURCES ../contracts/include/gocook/IGoCookApi.h
    SOURCES viewmodels/RecipeViewModel.cpp viewmodels/RecipeViewModel.h
    SOURCES viewmodels/AuthViewModel.cpp viewmodels/AuthViewModel.h
    SOURCES viewmodels/InventoryViewModel.cpp viewmodels/InventoryViewModel.h
    SOURCES viewmodels/NotificationViewModel.cpp viewmodels/NotificationViewModel.h
    SOURCES viewmodels/AnnouncementViewModel.cpp viewmodels/AnnouncementViewModel.h
    SOURCES viewmodels/ShoppingListViewModel.cpp viewmodels/ShoppingListViewModel.h
    SOURCES viewmodels/RequestGuards.h
    SOURCES dialogs/NativeFileDialog.h dialogs/NativeFileDialog.cpp
    SOURCES adapters/DataMapper.h adapters/DataMapper.cpp
    QML_FILES qml/pages/SearchPage.qml
    QML_FILES qml/pages/RecommendResultsPage.qml
    QML_FILES qml/pages/MyRecipesPage.qml
    QML_FILES qml/pages/MyRatingsPage.qml
    QML_FILES qml/pages/PreferencesPage.qml
    QML_FILES qml/pages/HealthProfilePage.qml
    QML_FILES qml/pages/ProfileEditPage.qml
    QML_FILES qml/pages/ChangePasswordPage.qml
    QML_FILES qml/pages/AccountSecurityPage.qml
    QML_FILES qml/pages/NotificationPage.qml
    QML_FILES qml/pages/NotificationDetailPage.qml
    QML_FILES qml/pages/SystemNoticePage.qml
    QML_FILES qml/pages/FeaturePlaceholderPage.qml
    QML_FILES qml/pages/SettingsPage.qml
    QML_FILES qml/components/TagChip.qml
    QML_FILES qml/components/SectionHeader.qml
    QML_FILES qml/components/StarRatingSelector.qml
    QML_FILES qml/components/ConfirmDialog.qml
    QML_FILES qml/components/ImagePickerBox.qml
    QML_FILES qml/components/RecipeIngredientItem.qml
    QML_FILES qml/components/RecipeStepItem.qml
    QML_FILES qml/components/ErrorBanner.qml
    QML_FILES qml/components/NetworkOfflineView.qml
    QML_FILES qml/components/GuestEmptyState.qml
    QML_FILES qml/components/NutritionSummaryCard.qml
    QML_FILES qml/components/VideoCardDelegate.qml
    QML_FILES qml/components/GroupSelectionDialog.qml
    QML_FILES qml/components/SimpleListInputDialog.qml
    QML_FILES qml/components/NotificationCardDelegate.qml
    QML_FILES qml/components/AnnouncementCard.qml
    QML_FILES qml/components/ShoppingListTableHeader.qml
)

# 增加头文件查找路径
# adapters：ViewModel 以裸名 #include <DataMapper.h> 引用适配层；
# viewmodels / dialogs：qmltyperegistrar 生成的注册代码同样以裸名 include QML_ELEMENT
# 头文件（<InventoryViewModel.h> / <NativeFileDialog.h>）——目录不在查找路径时，生成的
# 注册代码会因找不到类而编译失败（生成的 __has_include 守卫会静默跳过该 include）
target_include_directories(appclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../contracts/include)
target_include_directories(appclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)
target_include_directories(appclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/adapters)
target_include_directories(appclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels)
target_include_directories(appclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/dialogs)

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
        Qt6::Widgets
)

# 将 .desktop 文件拷贝到构建目录（portal 需要真实文件系统路径）
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/assets/gocook.desktop" "${CMAKE_CURRENT_BINARY_DIR}/gocook.desktop" COPYONLY)

# 嵌入占位图资源
set(_placeholder "${CMAKE_CURRENT_SOURCE_DIR}/assets/img-placeholder.svg")
set_source_files_properties("${_placeholder}" PROPERTIES QT_RESOURCE_ALIAS "img-placeholder.svg")
qt_add_resources(appclient "placeholder"
    PREFIX "/"
    FILES "${_placeholder}"
)

set_target_properties(appclient PROPERTIES
    AUTORCC TRUE                              #  set(CMAKE_AUTORCC ON)
    MACOSX_BUNDLE_GUI_IDENTIFIER com.gocook.app
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

# ---- 单元测试 ----
# GTest 缺失时跳过测试子目录（仅告警），避免无 GTest 的环境 configure 硬失败
# option 在顶层已有定义；此处保留是为了单独 configure 本目录时默认行为不变（no-op 不覆盖顶层值）
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_subdirectory(tests)
    else()
        message(WARNING "未找到 GTest，跳过单元测试。安装 GTest 可启用，或 -DBUILD_TESTING=OFF 关闭。")
    endif()
endif()


















































# set(CMAKE_CXX_STANDARD_REQUIRED ON) # 全局阻止 C++ 标准的回退行为
#qt_add_executable(client)  //创建并完成一个平台特定类型的应用目标。
#该命令执行以下任务：
#    1. 为目标平台创建适当类型的 CMake 目标。
#    2. 将目标链接到 Qt::Core 库。
#    3. 处理 CMake 目标的收尾工作。

# set(CMAKE_CXX_STANDARD_REQUIRED ON) # 全局阻止 C++ 标准的回退行为

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
# # 自 Qt 6.1 起，Qt 会在 iOS 上自动设置 MACOSX_BUNDLE_GUI_IDENTIFIER。
# # 如果你正在为 iOS 或 macOS 开发，应考虑手动设置一个
# # 明确、固定的 bundle 标识符。
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
