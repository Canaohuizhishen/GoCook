#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "ApiClient.h"
#include "LocalDatabase.h"
#include "AuthManager.h"

int main(int argc, char *argv[])
{
    //qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

    QGuiApplication app(argc, argv);

    // 注册 Theme 单例
    qmlRegisterSingletonType(
        QUrl("qrc:/client/qml/styles/Theme.qml"),  // 资源路径
        "client.styles",                            // 导入 URI
        1, 0,                                       // 版本号
        "Theme"                                     // QML 中的类型名
        );

    // 创建实例（单例模式，LocalDatabase 和 ApiClient 由 AuthManager 内部创建）
    AuthManager authManager;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("authManager", &authManager);
    engine.rootContext()->setContextProperty("apiClient", authManager.findChild<ApiClient*>());
    engine.rootContext()->setContextProperty("localDB", LocalDatabase::instance());

    const QUrl url(QStringLiteral("qrc:/client/qml/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    // 启动后自动检测登录状态
    authManager.checkAutoLogin();

    return app.exec();
}