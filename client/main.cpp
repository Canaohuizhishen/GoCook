#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "HttpGoCookApi.h"
#include "LocalDatabase.h"
#include "viewmodels/AuthViewModel.h"
#include "viewmodels/RecipeViewModel.h"

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

    // 创建具体 API 实现类实例，父对象设为 app 以确保生命周期
    HttpGoCookApi *httpApi = new HttpGoCookApi(&app);

    // 通过构造函数注入抽象接口，AuthViewModel 只依赖 GoCookApi 抽象
    AuthViewModel authViewModel(httpApi);

    // 创建 ViewModel 并注入 API 抽象接口
    RecipeViewModel recipeVM(httpApi, &app);

    // 暴露给 QML
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("authViewModel", &authViewModel);
    engine.rootContext()->setContextProperty("recipeVM", &recipeVM);

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
    authViewModel.checkAutoLogin();

    return app.exec();
}