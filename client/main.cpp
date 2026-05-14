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
        QUrl("qrc:/client/qml/styles/Theme.qml"),
        "client.styles",
        1, 0,
        "Theme"
        );

    // 创建具体 API 实现类实例（工厂模式——集中所有具体类实例化）
    HttpGoCookApi *httpApi = new HttpGoCookApi(&app);

    // AuthViewModel 通过抽象接口 IGoCookApi* 注入
    AuthViewModel authViewModel(httpApi);

    // RecipeViewModel 通过抽象接口 IGoCookApi* 注入
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

    authViewModel.checkAutoLogin();

    return app.exec();
}
