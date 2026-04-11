#include "AuthManager.h"
#include <QJSValue>
#include <QUrl>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

// 构造函数
AuthManager::AuthManager(QObject *parent) : QObject(parent),
    // 创建 ApiClient 实例并设为子对象
    m_api(new ApiClient(this)),
    // 获取本地数据库单例
    m_db(LocalDatabase::instance()),
    m_loggedIn(false),
    m_userId(0)
{
    // 连接 ApiClient 的未授权信号，自动执行登出
    connect(m_api, &ApiClient::unauthorized, this, [this]() {
        if (m_loggedIn) {
            logout();
        }
    });
}

// 登录实现（使用 ApiClient 的 std::function 重载）
void AuthManager::login(const QString &username, const QString &password)
{
    // 构造请求数据
    QVariantMap data;
    data["username"] = username;
    data["password"] = password;

    // 调用 ApiClient 的 post 方法，传入 C++ 回调
    m_api->post("/api/login", data, [this, username](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            emit loginFailed(errorMsg.isEmpty() ? "Unknown error" : errorMsg);
            return;
        }

        if (!doc.isObject()) {
            emit loginFailed("Invalid server response");
            return;
        }

        QJsonObject obj = doc.object();
        QString token = obj["token"].toString();
        int userId = obj["user_id"].toInt();

        // 检查令牌和用户 ID 是否有效
        if (token.isEmpty() || userId == 0) {
            emit loginFailed("Login failed: missing token or user_id");
            return;
        }

        // 保存用户信息到本地数据库
        m_db->saveUser(userId, username, token);
        // 设置 ApiClient 的令牌
        m_api->setToken(token);
        // 更新内部登录状态
        setLoggedIn(true, userId, username, token);
        // 发射登录成功信号
        emit loginSuccess();
    });
}

// 注册实现（使用 ApiClient 的 std::function 重载）
void AuthManager::registerUser(const QString &username, const QString &password)
{
    // 构造请求数据
    QVariantMap data;
    data["username"] = username;
    data["password"] = password;

    // 调用 ApiClient 的 post 方法，传入 C++ 回调
    m_api->post("/api/register", data, [this](bool success, const QString& errorMsg, const QJsonDocument& /*doc*/) {
        if (success) {
            emit registerSuccess();
        } else {
            emit registerFailed(errorMsg.isEmpty() ? "Unknown error" : errorMsg);
        }
    });
}

// 登出实现
void AuthManager::logout()
{
    // 清除本地用户数据
    m_db->clearUser();
    // 清除 ApiClient 的令牌
    m_api->setToken("");
    // 更新内部登录状态
    setLoggedIn(false, 0, "", "");
    // 发射登出完成信号
    emit logoutFinished();
}

// 自动登录检查
void AuthManager::checkAutoLogin()
{
    // 从本地数据库获取用户信息
    QVariantMap user = m_db->getUser();
    if (!user.isEmpty()) {
        QString token = user["token"].toString();
        // 可选：向服务器验证 token 有效性，此处简单信任本地
        m_api->setToken(token);
        // 恢复登录状态
        setLoggedIn(true, user["id"].toInt(), user["username"].toString(), token);
    }
}

// 内部状态更新方法
void AuthManager::setLoggedIn(bool loggedIn, int userId, const QString &username, const QString &token)
{
    Q_UNUSED(token)
    // 登录状态变化时发射信号
    if (m_loggedIn != loggedIn) {
        m_loggedIn = loggedIn;
        emit loggedInChanged();
    }
    // 用户 ID 变化时发射信号
    if (m_userId != userId) {
        m_userId = userId;
        emit userIdChanged();
    }
    // 用户名变化时发射信号
    if (m_username != username) {
        m_username = username;
        emit usernameChanged();
    }
}