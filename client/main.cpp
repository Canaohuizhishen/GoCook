#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "api/HttpGoCookApi.h"
#include "database/LocalDatabase.h"
#include "viewmodels/AuthViewModel.h"
#include "viewmodels/RecipeViewModel.h"
#include "viewmodels/InventoryViewModel.h"
#include "viewmodels/NotificationViewModel.h"
#include "viewmodels/AnnouncementViewModel.h"
#include "viewmodels/ShoppingListViewModel.h"

int main(int argc, char *argv[])
{
    //qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

    // 使用 Breeze 风格
    qputenv("QT_QUICK_CONTROLS_STYLE", "org.kde.breeze");

    QApplication app(argc, argv);

    // 设置桌面入口文件路径，用于 D-Bus 门户集成（文件对话框、通知等）
    // portal 需要真实文件系统路径来读取 .desktop 文件获取 app ID
    qputenv("QT_QPA_DESKTOP_ENTRY_PATH",
            (QCoreApplication::applicationDirPath() + "/gocook.desktop").toLocal8Bit());

    // QSettings 需要这些标识符来确定配置文件路径
    QCoreApplication::setOrganizationName("GoCook");
    QCoreApplication::setOrganizationDomain("gocook.app");

    // QML 类型注册全部声明式化（单一来源，qmltypes/qmllint 可静态解析），本文件不再命令式注册：
    //   - NativeFileDialog / InventoryViewModel：类内 QML_ELEMENT（后者配 QML_UNCREATABLE），
    //     由 qt_add_qml_module 生成的 qml_register_types_client() 在启动时自动注册；
    //   - Theme / PagePolicy 单例：client/CMakeLists.txt 的 QT_QML_SINGLETON_TYPE 声明，
    //     生成 qmldir 的 singleton 条目（资源内 :/client/qmldir），"import client" 即解析。

    HttpGoCookApi *httpApi = new HttpGoCookApi(&app);

    AuthViewModel authViewModel(httpApi);
    RecipeViewModel recipeVM(httpApi, &app);
    InventoryViewModel inventoryVM(httpApi, &app);
    NotificationViewModel notifyVM(httpApi, &app);
    AnnouncementViewModel announcementVM(httpApi, &app);
    ShoppingListViewModel shoppingListVM(httpApi, &app);

    // 网络恢复 → 库存自动同步（联网即同步；仅数据可能过期时重拉，见 VM）
    QObject::connect(httpApi, &HttpGoCookApi::networkRestored,
                     &inventoryVM, &InventoryViewModel::onNetworkRestored);

    // 会话结束（登出 / 401 自动登出 / 注销）：各 VM 个人数据统一清理——单一收束点，
    // 新增个人域只在此加一行（替代 QML 手工逐个调用）
    QObject::connect(&authViewModel, &AuthViewModel::sessionEnded, &inventoryVM, &InventoryViewModel::clearAll);
    QObject::connect(&authViewModel, &AuthViewModel::sessionEnded, &recipeVM, &RecipeViewModel::clearFavorites);
    QObject::connect(&authViewModel, &AuthViewModel::sessionEnded, &recipeVM, &RecipeViewModel::clearMyContent);
    QObject::connect(&authViewModel, &AuthViewModel::sessionEnded, &shoppingListVM, &ShoppingListViewModel::clearAll);
    QObject::connect(&authViewModel, &AuthViewModel::sessionEnded, &notifyVM, &NotificationViewModel::clearAll);
    QObject::connect(&authViewModel, &AuthViewModel::sessionEnded, &announcementVM, &AnnouncementViewModel::clearAll);

    // 登录成功 / 会话恢复：拉一次未读汇总，保证冷启动后铃铛角标与消息页卡片准确（游客静默跳过）
    QObject::connect(&authViewModel, &AuthViewModel::loggedInChanged, [&authViewModel, &notifyVM]() {
        if (authViewModel.loggedIn())
            notifyVM.refreshUnreadSummary();
    });

    // 系统通知页完成「看过」后：清除系统红点（消息页卡片与"我的"页铃铛共用该标记）
    QObject::connect(&announcementVM, &AnnouncementViewModel::announcementsSeen,
                     &notifyVM, &NotificationViewModel::clearSystemNewFlag);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("authViewModel", &authViewModel);
    engine.rootContext()->setContextProperty("httpApi", httpApi);
    engine.rootContext()->setContextProperty("recipeVM", &recipeVM);
    engine.rootContext()->setContextProperty("inventoryVM", &inventoryVM);
    engine.rootContext()->setContextProperty("notifyVM", &notifyVM);
    engine.rootContext()->setContextProperty("announcementVM", &announcementVM);
    engine.rootContext()->setContextProperty("shoppingListVM", &shoppingListVM);

    const QUrl url(QStringLiteral("qrc:/client/qml/Main.qml"));

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [](const QUrl &url) {
            qCritical("严重错误：QML 加载失败：%s", qPrintable(url.toString()));
        });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) {
                qCritical("严重错误：QML 对象创建失败（%s）", qPrintable(url.toString()));
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);

    engine.load(url);

    authViewModel.checkAutoLogin();

    return app.exec();
}
