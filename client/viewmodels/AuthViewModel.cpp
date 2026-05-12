#include "AuthViewModel.h"
#include <QJSValue>
#include <QUrl>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include "HttpGoCookApi.h"

AuthViewModel::AuthViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_db(LocalDatabase::instance())
    , m_loggedIn(false)
    , m_userId(0)
{
    // 通过抽象接口注册未授权回调，避免对具体实现类的 dynamic_cast 依赖
    m_api->setUnauthorizedHandler([this]() {
        if (m_loggedIn) { logout(); }
    });
}

// 登录实现（使用 GoCookApi 抽象接口的 login 方法）
void AuthViewModel::login(const QString &username, const QString &password)
{
    gocook::models::LoginRequest req;
    req.username = username.toStdString();
    req.password = password.toStdString();

    m_api->login(req, [this, username](bool success,
                                       const gocook::models::LoginResponse &data,
                                       const std::string &error) {
        if (!success) {
            emit loginFailed(QString::fromStdString(error.empty() ? "Unknown error" : error));
            return;
        }
        if (data.token.empty() || data.user_id == 0) {
            emit loginFailed("Login failed: missing token or user_id");
            return;
        }

        QString token = QString::fromStdString(data.token);
        QString apiUsername = QString::fromStdString(data.username);
        m_db->saveUser(data.user_id, apiUsername, token);
        m_api->setAuthToken(data.token);   // 直接传 std::string
        setLoggedIn(true, data.user_id, apiUsername, token);
        emit loginSuccess();
    });
}

// 注册实现（使用 GoCookApi 抽象接口的 registerUser 方法）
void AuthViewModel::registerUser(const QString &username,
                                 const QString &password,
                                 const QString &email)
{
    gocook::models::RegisterRequest req;
    req.username = username.toStdString();
    req.password = password.toStdString();
    req.email    = email.toStdString();   // 补充 email 字段

    m_api->registerUser(req, [this](bool success, const std::string &error) {
        if (success) {
            emit registerSuccess();
        } else {
            emit registerFailed(QString::fromStdString(
                error.empty() ? "Unknown error" : error));
        }
    });
}

// 登出实现
void AuthViewModel::logout()
{
    // 清除本地用户数据
    m_db->clearUser();
    // 清除 API 的认证令牌
    m_api->setAuthToken("");
    // 更新内部登录状态
    setLoggedIn(false, 0, "", "");
    // 发射登出完成信号
    emit logoutFinished();
}

// 自动登录检查
void AuthViewModel::checkAutoLogin()
{
    QVariantMap user = m_db->getUser();
    if (!user.isEmpty()) {
        QString token = user["token"].toString();
        m_api->setAuthToken(token.toStdString());
        setLoggedIn(true, user["id"].toInt(), user["username"].toString(), token);
    }
}

// 内部状态更新方法
void AuthViewModel::setLoggedIn(bool loggedIn, int userId, const QString &username, const QString &token)
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