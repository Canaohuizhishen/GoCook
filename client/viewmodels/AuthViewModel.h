#pragma once

#include <QObject>
#include "HttpGoCookApi.h"
#include "LocalDatabase.h"

/**
 * @brief 用户认证管理类，处理登录、注册、登出及自动登录逻辑
 *
 * 管理用户登录状态，与 HttpGoCookApi 和 LocalDatabase 交互，实现 Token 的持久化。
 * 提供 QML 可用的属性和方法，便于界面绑定和调用。
 */
class AuthViewModel : public QObject
{
    Q_OBJECT
    // 是否已登录
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    // 当前用户名
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)
    // 当前用户 ID
    Q_PROPERTY(int userId READ userId NOTIFY userIdChanged)

public:
    // 构造函数
    explicit AuthViewModel(HttpGoCookApi *api, QObject *parent = nullptr);

    // 获取登录状态
    bool loggedIn() const { return m_loggedIn; }
    // 获取用户名
    QString username() const { return m_username; }
    // 获取用户 ID
    int userId() const { return m_userId; }

    // 登录方法，供 QML 调用
    Q_INVOKABLE void login(const QString &username, const QString &password);
    // 注册方法，供 QML 调用
    Q_INVOKABLE void registerUser(const QString &username, const QString &password, const QString &email);
    // 登出方法，供 QML 调用
    Q_INVOKABLE void logout();
    // 检查自动登录状态，供 QML 调用
    Q_INVOKABLE void checkAutoLogin();

signals:
    // 登录状态变更信号
    void loggedInChanged();
    // 用户名变更信号
    void usernameChanged();
    // 用户 ID 变更信号
    void userIdChanged();
    // 登录成功信号
    void loginSuccess();
    // 登录失败信号，携带错误信息
    void loginFailed(const QString &error);
    // 注册成功信号
    void registerSuccess();
    // 注册失败信号，携带错误信息
    void registerFailed(const QString &error);
    // 登出完成信号
    void logoutFinished();

private:
    // 内部方法：设置登录状态并更新相关属性
    void setLoggedIn(bool loggedIn, int userId = 0, const QString &username = "", const QString &token = "");

    // API 实现指针
    HttpGoCookApi *m_api;
    // 本地数据库单例
    LocalDatabase *m_db;
    // 登录状态标志
    bool m_loggedIn;
    // 当前用户 ID
    int m_userId;
    // 当前用户名
    QString m_username;
};