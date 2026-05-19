#pragma once

#include <QObject>
#include <gocook/IGoCookApi.h>
#include "../database/LocalDatabase.h"

/**
 * @brief 用户认证管理类，处理登录、注册、登出及自动登录逻辑
 *
 * 管理用户登录状态，与 IGoCookApi 抽象接口和 LocalDatabase 交互，实现 Token 的持久化。
 * 提供 QML 可用的属性和方法，便于界面绑定和调用。
 */
class AuthViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)
    Q_PROPERTY(int userId READ userId NOTIFY userIdChanged)
    Q_PROPERTY(bool initialLoading READ initialLoading NOTIFY initialLoadingChanged)

public:
    explicit AuthViewModel(IGoCookApi *api, QObject *parent = nullptr);

    bool loggedIn() const { return m_loggedIn; }
    QString username() const { return m_username; }
    int userId() const { return m_userId; }
    bool initialLoading() const { return m_initialLoading; }

    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void registerUser(const QString &username, const QString &password, const QString &email);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void checkAutoLogin();

signals:
    void loggedInChanged();
    void usernameChanged();
    void userIdChanged();
    void loginSuccess();
    void loginFailed(const QString &error);
    void registerSuccess();
    void registerFailed(const QString &error);
    void logoutFinished();
    void initialLoadingChanged();

private:
    void setLoggedIn(bool loggedIn, int userId = 0, const QString &username = "");

    IGoCookApi *m_api;
    LocalDatabase *m_db;
    bool m_loggedIn;
    int m_userId;
    QString m_username;
    bool m_initialLoading = true;
};
