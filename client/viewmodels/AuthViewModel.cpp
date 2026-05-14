#include "AuthViewModel.h"
#include <QJSValue>
#include <QUrl>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QPointer>
#include "HttpGoCookApi.h"

AuthViewModel::AuthViewModel(HttpGoCookApi *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_db(LocalDatabase::instance())
    , m_loggedIn(false)
    , m_userId(0)
{
    // 通过抽象接口注册未授权回调，避免对具体实现类的 dynamic_cast 依赖
    m_api->setUnauthorizedHandler([self = QPointer<AuthViewModel>(this)]() {
        if (!self) return;
        if (self->m_loggedIn) { self->logout(); }
    });
}

// 登录实现（使用 GoCookApi 抽象接口的 login 方法）
void AuthViewModel::login(const QString &username, const QString &password)
{
    gocook::models::LoginRequest req;
    req.username = username.toStdString();
    req.password = password.toStdString();

    m_api->login(req, [self = QPointer<AuthViewModel>(this), username](bool success,
                                        const gocook::models::LoginResponse &data,
                                        const std::string &error) {
        if (!self) return;
        if (!success) {
            emit self->loginFailed(QString::fromStdString(error.empty() ? "Unknown error" : error));
            return;
        }
        if (data.token.empty() || data.user_id == 0) {
            emit self->loginFailed("Login failed: missing token or user_id");
            return;
        }

        QString token = QString::fromStdString(data.token);
        QString apiUsername = QString::fromStdString(data.username);
        self->m_db->saveUser(data.user_id, apiUsername, token);
        self->m_api->setAuthToken(data.token);
        self->setLoggedIn(true, data.user_id, apiUsername);
        emit self->loginSuccess();
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

    m_api->registerUser(req, [self = QPointer<AuthViewModel>(this)](bool success, const std::string &error) {
        if (!self) return;
        if (success) {
            emit self->registerSuccess();
        } else {
            emit self->registerFailed(QString::fromStdString(
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
    setLoggedIn(false, 0, "");
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
        // 异步验证本地缓存的令牌是否仍然有效
        m_api->getCurrentUser([self = QPointer<AuthViewModel>(this)](bool success, const gocook::models::UserProfile& profile, const std::string& error) {
            Q_UNUSED(profile)
            if (!self) return;
            if (success) {
                QVariantMap u = self->m_db->getUser();
                self->setLoggedIn(true, u["id"].toInt(), u["username"].toString());
            } else {
                self->m_db->clearUser();
                self->m_api->setAuthToken("");
                if (self->m_loggedIn) {
                    self->setLoggedIn(false, 0, "");
                }
            }
            self->m_initialLoading = false;
            emit self->initialLoadingChanged();  // 触发 QML 从加载页切换到登录页或主页
        });
    } else {
        // 本地无缓存令牌，直接结束加载状态
        m_initialLoading = false;
        emit initialLoadingChanged();
    }
}

// 内部状态更新方法
void AuthViewModel::setLoggedIn(bool loggedIn, int userId, const QString &username)
{
    if (m_loggedIn != loggedIn) {
        m_loggedIn = loggedIn;
        emit loggedInChanged();
    }
    if (m_userId != userId) {
        m_userId = userId;
        emit userIdChanged();
    }
    if (m_username != username) {
        m_username = username;
        emit usernameChanged();
    }
}