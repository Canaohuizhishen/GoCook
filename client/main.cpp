#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "api/HttpGoCookApi.h"
#include "database/LocalDatabase.h"
#include "viewmodels/AuthViewModel.h"
#include "viewmodels/RecipeViewModel.h"
#include "viewmodels/InventoryViewModel.h"
#include "viewmodels/NotificationViewModel.h"

int main(int argc, char *argv[])
{
    //qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

    // Use Fusion style instead of Breeze — Breeze's ButtonBackground.qml:19
    // assumes the background item's parent is always a T.AbstractButton, which
    // breaks when our custom Button.background is set.
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");

    QGuiApplication app(argc, argv);

    // QSettings needs these identifiers to determine config file paths
    QCoreApplication::setOrganizationName("GoCook");
    QCoreApplication::setOrganizationDomain("gocook.app");

    // Register Theme.qml as a singleton under the "client" module URI.
    // Theme.qml is also listed in qt_add_qml_module QML_FILES; the manual
    // registration makes it accessible via "import client" in non-module files.
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

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("authViewModel", &authViewModel);
    engine.rootContext()->setContextProperty("recipeVM", &recipeVM);
    engine.rootContext()->setContextProperty("inventoryVM", &inventoryVM);
    engine.rootContext()->setContextProperty("notifyVM", &notifyVM);

    const QUrl url(QStringLiteral("qrc:/client/qml/Main.qml"));

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [](const QUrl &url) {
            qCritical("CRITICAL: Failed to load QML: %s", qPrintable(url.toString()));
        });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) {
                qCritical("CRITICAL: objectCreated returned null for %s", qPrintable(url.toString()));
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);

    engine.load(url);

    authViewModel.checkAutoLogin();

    return app.exec();
}
