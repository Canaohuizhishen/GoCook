#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "api/HttpGoCookApi.h"
#include "database/LocalDatabase.h"
#include "viewmodels/AuthViewModel.h"
#include "viewmodels/RecipeViewModel.h"
#include "viewmodels/InventoryViewModel.h"
#include "viewmodels/NotificationViewModel.h"
#include "viewmodels/ShoppingListViewModel.h"
#include "dialogs/NativeFileDialog.h"

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

    // 注册 C++ 类型供 QML 使用
    qmlRegisterType<NativeFileDialog>("client", 1, 0, "NativeFileDialog");
    // 注册库存 VM 类型（不可实例化）：供 QML 访问 ViewState 枚举（如 InventoryViewModel.Offline）
    qmlRegisterUncreatableType<InventoryViewModel>("client", 1, 0, "InventoryViewModel",
                                                   "仅用于枚举访问（视图状态），不可实例化");

    // 将 Theme.qml 注册为 "client" 模块下的单例
    // Theme.qml 已在 qt_add_qml_module 的 QML_FILES 中列出；此处手动注册
    // 使其在非 module 文件中也可通过 "import client" 访问
    qmlRegisterSingletonType(
        QUrl("qrc:/client/qml/styles/Theme.qml"),
        "client",
        1, 0,
        "Theme"
        );

    HttpGoCookApi *httpApi = new HttpGoCookApi(&app);

    AuthViewModel authViewModel(httpApi);
    RecipeViewModel recipeVM(httpApi, &app);
    InventoryViewModel inventoryVM(httpApi, &app);
    NotificationViewModel notifyVM(httpApi, &app);
    ShoppingListViewModel shoppingListVM(httpApi, &app);

    // 网络恢复 → 库存自动同步（联网即同步；仅数据可能过期时重拉，见 VM）
    QObject::connect(httpApi, &HttpGoCookApi::networkRestored,
                     &inventoryVM, &InventoryViewModel::onNetworkRestored);

    // 会话结束（登出 / 401 自动登出 / 注销）：各 VM 个人数据统一清理——单一收束点，
    // 新增个人域只在此加一行（替代 QML 手工逐个调用）
    QObject::connect(&authViewModel, &AuthViewModel::sessionEnded, &inventoryVM, &InventoryViewModel::clearAll);
    QObject::connect(&authViewModel, &AuthViewModel::sessionEnded, &recipeVM, &RecipeViewModel::clearFavorites);
    QObject::connect(&authViewModel, &AuthViewModel::sessionEnded, &shoppingListVM, &ShoppingListViewModel::clearAll);
    QObject::connect(&authViewModel, &AuthViewModel::sessionEnded, &notifyVM, &NotificationViewModel::clearAll);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("authViewModel", &authViewModel);
    engine.rootContext()->setContextProperty("httpApi", httpApi);
    engine.rootContext()->setContextProperty("recipeVM", &recipeVM);
    engine.rootContext()->setContextProperty("inventoryVM", &inventoryVM);
    engine.rootContext()->setContextProperty("notifyVM", &notifyVM);
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
