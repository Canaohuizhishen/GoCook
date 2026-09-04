#include "HttpGoCookApi.h"
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJSValue>
#include <QJSEngine>
#include <QtQml/QQmlEngine>
#include <QTimer>
#include <QPointer>
#include <QFile>
#include <QFileInfo>
#include <gocook/IServices.h>

// 统一错误文案解析：三级判据（与笔记 6.1 一致）
//  1) statusCode <= 0        → 网络层错误（断网/拒绝连接/超时），无服务端响应
//  2) 服务端 JSON 含 error   → 业务错误，用服务端精确文案
//  3) 其余（statusCode > 0） → 服务器问题（5xx 返回 HTML/空体等），不能误导用户去查网络
static QString errorMessageFor(int statusCode, const QJsonDocument &doc)
{
    // -2 = 被登录守卫拦截（未登录且接口需登录），未发送任何请求
    if (statusCode == -2) {
        return HttpGoCookApi::kAuthRequiredError;
    }
    if (statusCode <= 0) {
        return QStringLiteral("网络连接失败，请检查网络");
    }
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.contains("error")) {
            const QString err = obj["error"].toString();
            if (!err.isEmpty())
                return err;
        }
    }
    return QStringLiteral("服务器有点问题，请稍候再试");
}

// 构造函数
HttpGoCookApi::HttpGoCookApi(QObject *parent) : QObject(parent)
{
    // 初始化默认基础 URL
    m_baseUrl = "http://127.0.0.1:8080";
}

// 设置基础 URL
void HttpGoCookApi::setBaseUrl(const QString &url)
{
    if (m_baseUrl != url) {
        m_baseUrl = url;
        // 发射属性变更信号
        emit baseUrlChanged();
    }
}

// 设置认证令牌
void HttpGoCookApi::setToken(const QString &token)
{
    if (m_token != token) {
        m_token = token;
        // 发射属性变更信号
        emit tokenChanged();
        // 登录守卫联动：拿到新 token（登录成功）→ 重放挂起的 Interactive 请求；
        // token 被清空（登出）→ 挂起请求作废，按"请先登录"回调失败
        if (!token.isEmpty())
            replayPendingAuthRequests();
        else
            clearPendingAuthRequests();
    }
}

// 登录成功后重放全部挂起的 Interactive 请求（按入队顺序；此时 token 已非空，不会再次被拦截）
void HttpGoCookApi::replayPendingAuthRequests()
{
    if (m_pendingAuthRequests.empty())
        return;
    auto pending = std::move(m_pendingAuthRequests);
    m_pendingAuthRequests.clear();
    for (auto &p : pending) {
        // 挂起时 token 为空，request 上未携带 Authorization 头——重放前补上，否则服务端返回 401
        p.request.setRawHeader("Authorization", QString("Bearer %1").arg(m_token).toUtf8());
        sendRaw(AuthMode::Interactive, p.op, p.request, p.body, p.methodOverride, std::move(p.handler));
    }
}

// 清空挂起队列：每个请求按 -2"请先登录"回调失败（登录页被关闭/跳过，或登出时）
void HttpGoCookApi::clearPendingAuthRequests()
{
    if (m_pendingAuthRequests.empty())
        return;
    auto pending = std::move(m_pendingAuthRequests);
    m_pendingAuthRequests.clear();
    for (auto &p : pending) {
        if (p.handler)
            p.handler(-2, QByteArray());
    }
}

// QML 可调用：取消全部挂起的 Interactive 请求（应用内登录页被关闭/跳过时）
void HttpGoCookApi::cancelAuthQueue()
{
    clearPendingAuthRequests();
}

// 设置最大重试次数
void HttpGoCookApi::setMaxRetries(int retries)
{
    if (m_maxRetries != retries) {
        m_maxRetries = retries;
        emit maxRetriesChanged();
    }
}

// 设置重试延迟
void HttpGoCookApi::setRetryDelay(int delayMs)
{
    if (m_retryDelay != delayMs) {
        m_retryDelay = delayMs;
        emit retryDelayChanged();
    }
}

// GET 请求封装（QML 版本）
void HttpGoCookApi::get(const QString &endpoint, const QJSValue &callback, AuthMode authMode)
{
    // 调用统一请求发送方法，retryCount 从 0 开始
    sendRequest(QNetworkAccessManager::GetOperation, endpoint, QVariantMap(), callback, 0, "", false, authMode);
}

// GET 请求封装（C++ 版本，std::function 回调）
// suppressNetworkError=true：失败时不发全局 networkError（页面自行呈现离线状态）
void HttpGoCookApi::get(const QString &endpoint,
                        std::function<void(bool, const QString&, const QJsonDocument&)> callback,
                        bool suppressNetworkError,
                        AuthMode authMode)
{
    sendRequest(QNetworkAccessManager::GetOperation, endpoint, QVariantMap(), std::move(callback), 0, "", suppressNetworkError, authMode);
}

// POST 请求封装（QML 版本）
void HttpGoCookApi::post(const QString &endpoint, const QVariantMap &data, const QJSValue &callback, AuthMode authMode)
{
    sendRequest(QNetworkAccessManager::PostOperation, endpoint, data, callback, 0, "", false, authMode);
}

// POST 请求封装（C++ 版本，std::function 回调）
void HttpGoCookApi::post(const QString &endpoint, const QVariantMap &data,
                         std::function<void(bool, const QString&, const QJsonDocument&)> callback,
                         bool suppressNetworkError,
                         AuthMode authMode)
{
    sendRequest(QNetworkAccessManager::PostOperation, endpoint, data, std::move(callback), 0, "", suppressNetworkError, authMode);
}

// DELETE 请求封装（QML 版本）
void HttpGoCookApi::deleteResource(const QString &endpoint, const QVariantMap &data, const QJSValue &callback, AuthMode authMode)
{
    sendRequest(QNetworkAccessManager::DeleteOperation, endpoint, data, callback, 0, "", false, authMode);
}

// DELETE 请求封装（C++ 版本，std::function 回调）
void HttpGoCookApi::deleteResource(const QString &endpoint, const QVariantMap &data,
                                   std::function<void(bool, const QString&, const QJsonDocument&)> callback,
                                   bool suppressNetworkError,
                                   AuthMode authMode)
{
    sendRequest(QNetworkAccessManager::DeleteOperation, endpoint, data, std::move(callback), 0, "", suppressNetworkError, authMode);
}

// PUT 请求封装（QML 版本）
void HttpGoCookApi::put(const QString &endpoint, const QVariantMap &data, const QJSValue &callback, AuthMode authMode)
{
    sendRequest(QNetworkAccessManager::PutOperation, endpoint, data, callback, 0, "", false, authMode);
}

// PUT 请求封装（C++ 版本，std::function 回调）
void HttpGoCookApi::put(const QString &endpoint, const QVariantMap &data,
                        std::function<void(bool, const QString&, const QJsonDocument&)> callback,
                        bool suppressNetworkError,
                        AuthMode authMode)
{
    sendRequest(QNetworkAccessManager::PutOperation, endpoint, data, std::move(callback), 0, "", suppressNetworkError, authMode);
}

// PATCH 请求封装（QML 版本）
void HttpGoCookApi::patch(const QString &endpoint, const QVariantMap &data, const QJSValue &callback, AuthMode authMode)
{
    sendRequest(QNetworkAccessManager::CustomOperation, endpoint, data, callback, 0, "PATCH", false, authMode);
}

// PATCH 请求封装（C++ 版本，std::function 回调）
void HttpGoCookApi::patch(const QString &endpoint, const QVariantMap &data,
                          std::function<void(bool, const QString&, const QJsonDocument&)> callback,
                          bool suppressNetworkError,
                          AuthMode authMode)
{
    sendRequest(QNetworkAccessManager::CustomOperation, endpoint, data, std::move(callback), 0, "PATCH", suppressNetworkError, authMode);
}

// 构造标准 JSON 请求（URL、15 秒超时、Content-Type、Authorization）
QNetworkRequest HttpGoCookApi::buildRequest(const QString &endpoint, const QString &methodOverride)
{
    // 构造完整 URL
    QUrl url(m_baseUrl + endpoint);
    QNetworkRequest request(url);
    // 设置传输超时（15 秒），避免请求无限挂起
    request.setTransferTimeout(15000);
    // 设置内容类型头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // 如果存在令牌，则添加 Authorization 头
    if (!m_token.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_token).toUtf8());
    }
    return request;
}

// 统一底层发送原语：所有 HTTP 请求（含文件上传等手写路径）的唯一出口
void HttpGoCookApi::sendRaw(AuthMode authMode,
                            QNetworkAccessManager::Operation op,
                            QNetworkRequest &request,
                            const QByteArray &body,
                            const QString &methodOverride,
                            std::function<void(int statusCode, const QByteArray &responseData)> handler)
{
    // ===== 登录守卫 preflight：未登录且接口声明需登录 → 不发送请求 =====
    // Interactive：挂起入队并 emit authRequired()（上层弹应用内登录页）；
    //               登录成功（tokenChanged）后按入队顺序自动重放；登录页被关闭
    //               （cancelAuthQueue）或登出时按 -2"请先登录"回调失败。
    // Silent：直接按 -2 回调失败（页面呈现"登录后可用"空态）。
    // 此处保证绝不发出未授权请求。
    if (authMode != AuthMode::Public && m_token.isEmpty()) {
        if (authMode == AuthMode::Interactive) {
            m_pendingAuthRequests.push_back(
                PendingAuthRequest{op, request, body, methodOverride, std::move(handler)});
            emit authRequired();
            return;
        }
        if (handler) handler(-2, QByteArray());
        return;
    }

    // 根据操作类型发送请求
    QNetworkReply *reply = nullptr;
    switch (op) {
    case QNetworkAccessManager::GetOperation:
        reply = m_nam.get(request);
        break;
    case QNetworkAccessManager::PostOperation:
        reply = m_nam.post(request, body);
        break;
    case QNetworkAccessManager::DeleteOperation:
        reply = m_nam.sendCustomRequest(request, "DELETE", body);
        break;
    case QNetworkAccessManager::PutOperation:
        reply = m_nam.put(request, body);
        break;
    case QNetworkAccessManager::CustomOperation:
        reply = m_nam.sendCustomRequest(request,
            methodOverride.isEmpty() ? "PATCH" : methodOverride.toUtf8().constData(), body);
        break;
    default:
        break;
    }
    if (!reply) {
        // 请求未能发出（op 非法等）：按网络层错误回调，由调用方决定文案
        if (handler) handler(-1, QByteArray());
        return;
    }

    // 连接请求完成信号，QPointer 守卫防止对象销毁后 λ 访问已释放内存
    connect(reply, &QNetworkReply::finished, this, [self = QPointer<HttpGoCookApi>(this), reply, handler]() {
        if (!self) {
            reply->deleteLater();
            return;
        }
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray responseData = reply->readAll();
        reply->deleteLater();
        // 401 统一触发未授权信号（全局登出/游客兜底），业务解析留给 handler
        if (statusCode == 401) {
            emit self->unauthorized();
            self->invokeUnauthorizedHandler();
        }
        if (handler) handler(statusCode, responseData);
    });
}

// 统一发送 HTTP 请求的实现（QJSValue 回调版本），委托给 std::function 版本
void HttpGoCookApi::sendRequest(QNetworkAccessManager::Operation op,
                                const QString &endpoint,
                                const QVariantMap &data,
                                const QJSValue &callback,
                                int retryCount,
                                const QString &methodOverride,
                                bool suppressNetworkError,
                                AuthMode authMode)
{
    auto wrapped = [this, callback](bool success, const QString&, const QJsonDocument& doc) {
        if (!callback.isCallable()) return;
        QJSEngine *engine = qjsEngine(this);
        QJSValue jsResponse;
        if (engine) {
            if (doc.isObject()) {
                jsResponse = engine->toScriptValue(doc.object().toVariantMap());
            } else if (doc.isArray()) {
                QJsonArray array = doc.array();
                jsResponse = engine->newArray(array.size());
                for (int i = 0; i < array.size(); ++i) {
                    QJsonValue value = array[i];
                    if (value.isObject()) {
                        jsResponse.setProperty(i, engine->toScriptValue(value.toObject().toVariantMap()));
                    } else if (value.isArray()) {
                        jsResponse.setProperty(i, engine->toScriptValue(value.toArray().toVariantList()));
                    } else {
                        jsResponse.setProperty(i, engine->toScriptValue(value.toVariant()));
                    }
                }
            } else {
                jsResponse = engine->newObject();
            }
        } else {
            jsResponse = QJSValue(QJSValue::NullValue);
        }
        QJSValueList args;
        args << success;
        args << (success ? QString() : QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        args << jsResponse;
        QJSValue(callback).call(args);
    };
    sendRequest(op, endpoint, data, wrapped, retryCount, methodOverride, suppressNetworkError, authMode);
}

// 统一发送 HTTP 请求的实现（std::function 回调版本），retryCount 用于重试控制
void HttpGoCookApi::sendRequest(QNetworkAccessManager::Operation op,
                                const QString &endpoint,
                                const QVariantMap &data,
                                std::function<void(bool, const QString&, const QJsonDocument&)> callback,
                                int retryCount,
                                const QString &methodOverride,
                                bool suppressNetworkError,
                                AuthMode authMode)
{
    QNetworkRequest request = buildRequest(endpoint, methodOverride);
    // 将数据转换为 JSON 字节数组
    QByteArray body;
    if (!data.isEmpty()) {
        body = QJsonDocument(QJsonObject::fromVariantMap(data)).toJson();
    }

    sendRaw(authMode, op, request, body, methodOverride,
            [self = QPointer<HttpGoCookApi>(this), op, endpoint, data, callback, retryCount, methodOverride, suppressNetworkError, authMode]
            (int statusCode, const QByteArray &responseData) {
        if (!self) return;
        QJsonDocument doc = QJsonDocument::fromJson(responseData);

        // -2 = 被登录守卫拦截（未登录且接口需登录）：请求未发出，静默失败（不发全局网络错误、不重试）
        if (statusCode == -2) {
            if (callback) callback(false, errorMessageFor(statusCode, doc), doc);
            return;
        }

        // 401：全局 unauthorized 信号已由 sendRaw 统一发出，此处仅回调失败（服务端精确文案）
        if (statusCode == 401) {
            if (callback) callback(false, errorMessageFor(statusCode, doc), doc);
            return;
        }

        // 网络层错误（无任何 HTTP 响应，statusCode<=0）：只对 GET 重试；
        // 4xx/5xx 说明服务端已应答（包裹送到了），业务错误/服务器问题重试只会放大问题
        if (statusCode <= 0) {
            if (op == QNetworkAccessManager::GetOperation && retryCount < self->m_maxRetries) {
                QTimer::singleShot(self->m_retryDelay, self, [self, op, endpoint, data, callback, retryCount, suppressNetworkError, authMode]() {
                    if (!self) return;
                    self->sendRequest(op, endpoint, data, callback, retryCount + 1, "", suppressNetworkError, authMode);
                });
                return;
            }

            const QString errMsg = errorMessageFor(statusCode, doc);
            if (!suppressNetworkError) emit self->networkError(errMsg);
            if (callback) callback(false, errMsg, doc);
            return;
        }

        bool success = (statusCode >= 200 && statusCode < 300);
        if (success) {
            if (callback) callback(true, QString(), doc);
        } else {
            const QString errMsg = errorMessageFor(statusCode, doc);
            if (!suppressNetworkError) emit self->networkError(errMsg);
            if (callback) callback(false, errMsg, doc);
        }
    });
}

// ==================== GoCookApi 抽象接口实现 ======================

// ======================= 认证 =======================
void HttpGoCookApi::registerUser(const gocook::models::RegisterRequest& request,
                                 SuccessCallback callback)
{
    QVariantMap data;
    data["username"] = QString::fromStdString(request.username);
    data["password"] = QString::fromStdString(request.password);
    data["email"]    = QString::fromStdString(request.email);

    post("/api/register", data, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (success) {
            callback(true, "");
        } else {
            QString err = errorMsg;
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error")) {
                    err = obj["error"].toString();
                }
            }
            callback(false, err.isEmpty() ? "未知错误" : err.toStdString());
        }
    }, true);
}

void HttpGoCookApi::login(const gocook::models::LoginRequest& request,
                          LoginCallback callback)
{
    QVariantMap data;
    data["username"] = QString::fromStdString(request.username);
    data["password"] = QString::fromStdString(request.password);

    post("/api/login", data, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            callback(false, gocook::models::LoginResponse{},
                     errorMsg.isEmpty() ? "未知错误" : errorMsg.toStdString());
            return;
        }
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            gocook::models::LoginResponse resp;
            resp.token = obj["token"].toString().toStdString();
            resp.user_id = obj["user_id"].toInt();
            resp.username = obj["username"].toString().toStdString();
            callback(true, resp, "");
        } else {
            callback(false, gocook::models::LoginResponse{}, "响应格式无效");
        }
    }, true);
}

void HttpGoCookApi::forgotPassword(const std::string& username,
                                   const std::string& email,
                                   SuccessCallback callback)
{
    QVariantMap data;
    data["username"] = QString::fromStdString(username);
    data["email"] = QString::fromStdString(email);

    post("/api/password/forgot", data, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (success) {
            callback(true, "");
        } else {
            QString err = errorMsg;
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error"))
                    err = obj["error"].toString();
            }
            callback(false, err.isEmpty() ? "请求失败" : err.toStdString());
        }
    }, true);
}

void HttpGoCookApi::resetPassword(const std::string& token,
                                  const std::string& newPassword,
                                  SuccessCallback callback)
{
    QVariantMap data;
    data["token"] = QString::fromStdString(token);
    data["new_password"] = QString::fromStdString(newPassword);

    post("/api/password/reset", data, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (success) {
            callback(true, "");
        } else {
            QString err = errorMsg;
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error"))
                    err = obj["error"].toString();
            }
            callback(false, err.isEmpty() ? "重置失败" : err.toStdString());
        }
    }, true);
}

// ======================= 用户相关 =======================
void HttpGoCookApi::getCurrentUser(UserProfileCallback callback) {
    // Silent：登录态校验/加载类，未登录静默失败（checkAutoLogin 仅在本地有 token 时调用，不受影响）
    get("/api/users/me", [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            callback(false, gocook::models::UserProfile{}, errorStr.toStdString());
            return;
        }
        QJsonObject obj = doc.object();
        gocook::models::UserProfile profile;
        profile.id = obj["id"].toInt();
        profile.username = obj["username"].toString().toStdString();
        profile.display_name = obj["display_name"].toString().toStdString();
        profile.email = obj["email"].toString().toStdString();
        profile.phone = obj["phone"].toString().toStdString();
        profile.avatar_url = obj["avatar_url"].toString().toStdString();
        profile.preferences_complete = obj["preferences_complete"].toBool();
        profile.created_at = obj["created_at"].toString().toStdString();
        callback(true, profile, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::updateProfile(const gocook::models::UpdateProfileRequest& profile,
                                  UserProfileCallback callback) {
    QVariantMap data;
    if (profile.display_name.has_value())
        data["display_name"] = QString::fromStdString(profile.display_name.value());
    if (profile.email.has_value())
        data["email"] = QString::fromStdString(profile.email.value());
    if (profile.phone.has_value())
        data["phone"] = QString::fromStdString(profile.phone.value());
    if (profile.avatar_url.has_value())
        data["avatar_url"] = QString::fromStdString(profile.avatar_url.value());
    if (profile.avatar_id.has_value())
        data["avatar_id"] = profile.avatar_id.value();

    put("/api/users/me/profile", data, [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            QString err = errorStr;
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error"))
                    err = obj["error"].toString();
            }
            if (callback) callback(false, gocook::models::UserProfile{}, err.toStdString());
            return;
        }
        QJsonObject obj = doc.object();
        gocook::models::UserProfile profile;
        profile.id = obj["id"].toInt();
        profile.username = obj["username"].toString().toStdString();
        profile.display_name = obj["display_name"].toString().toStdString();
        profile.email = obj["email"].toString().toStdString();
        profile.phone = obj["phone"].toString().toStdString();
        profile.avatar_url = obj["avatar_url"].toString().toStdString();
        profile.preferences_complete = obj["preferences_complete"].toBool();
        profile.created_at = obj["created_at"].toString().toStdString();
        if (callback) callback(true, profile, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::getPreferences(PreferencesCallback callback) {
    get("/api/users/me/preferences", [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::UserPreferences{}, errorStr.toStdString());
            return;
        }
        QJsonObject obj = doc.object();
        gocook::models::UserPreferences prefs;
        if (obj.contains("likes")) {
            for (const auto& v : obj["likes"].toArray())
                prefs.likes.push_back(v.toString().toStdString());
        }
        if (obj.contains("dislikes")) {
            for (const auto& v : obj["dislikes"].toArray())
                prefs.dislikes.push_back(v.toString().toStdString());
        }
        if (obj.contains("health_goal"))
            prefs.health_goal = obj["health_goal"].toString().toStdString();
        if (callback) callback(true, prefs, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::updatePreferences(const gocook::models::UserPreferences& prefs,
                                      SuccessCallback callback) {
    QVariantMap data;
    {
        QStringList likes;
        for (const auto& v : prefs.likes) likes << QString::fromStdString(v);
        data["likes"] = likes;
    }
    {
        QStringList dislikes;
        for (const auto& v : prefs.dislikes) dislikes << QString::fromStdString(v);
        data["dislikes"] = dislikes;
    }
    data["health_goal"] = QString::fromStdString(prefs.health_goal);

    put("/api/users/me/preferences", data, [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            QString err = errorStr;
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error"))
                    err = obj["error"].toString();
            }
            if (callback) callback(false, err.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::updateHealthProfile(const gocook::models::HealthProfileRequest& healthProfile,
                                        HealthProfileCallback callback) {
    QVariantMap data;
    if (healthProfile.height_cm.has_value())
        data["height_cm"] = healthProfile.height_cm.value();
    if (healthProfile.weight_kg.has_value())
        data["weight_kg"] = healthProfile.weight_kg.value();
    {
        QStringList conditions;
        for (const auto& v : healthProfile.conditions)
            conditions << QString::fromStdString(v);
        data["conditions"] = conditions;
    }

    put("/api/users/me/health-profile", data, [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            QString err = errorStr;
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error"))
                    err = obj["error"].toString();
            }
            if (callback) callback(false, gocook::models::HealthProfileResponse{}, err.toStdString());
            return;
        }

        gocook::models::HealthProfileResponse resp;
        QJsonObject obj = doc.object();
        if (obj.contains("suggested_avoidances")) {
            QJsonArray arr = obj["suggested_avoidances"].toArray();
            for (const auto& v : arr) {
                QJsonObject item = v.toObject();
                gocook::models::AvoidanceItem ai;
                ai.ingredient = item["ingredient"].toString().toStdString();
                ai.reason = item["reason"].toString().toStdString();
                resp.suggested_avoidances.push_back(ai);
            }
        }
        if (callback) callback(true, resp, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::getHealthProfile(HealthProfileCallback callback) {
    get("/api/users/me/health-profile", [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::HealthProfileResponse{}, errorStr.toStdString());
            return;
        }

        gocook::models::HealthProfileResponse resp;
        QJsonObject obj = doc.object();
        if (obj.contains("height_cm"))
            resp.height_cm = obj["height_cm"].toInt();
        if (obj.contains("weight_kg"))
            resp.weight_kg = obj["weight_kg"].toDouble();
        if (obj.contains("conditions")) {
            for (const auto& v : obj["conditions"].toArray())
                resp.conditions.push_back(v.toString().toStdString());
        }
        if (obj.contains("suggested_avoidances")) {
            QJsonArray arr = obj["suggested_avoidances"].toArray();
            for (const auto& v : arr) {
                QJsonObject item = v.toObject();
                gocook::models::AvoidanceItem ai;
                ai.ingredient = item["ingredient"].toString().toStdString();
                ai.reason = item["reason"].toString().toStdString();
                resp.suggested_avoidances.push_back(ai);
            }
        }
        if (callback) callback(true, resp, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::uploadAvatar(const std::string& filePath,
                                 AvatarUploadCallback callback)
{
    // 调试日志写到文件 /tmp/gocook_avatar_debug.log
    auto avLog = [](const QString& msg) {
        QFile f("/tmp/gocook_avatar_debug.log");
        if (f.open(QIODevice::Append | QIODevice::Text)) {
            f.write(("[CLIENT] " + msg + "\n").toUtf8());
            f.close();
        }
    };
    avLog("=== 头像上传开始 ===");
    avLog("文件路径：" + QString::fromStdString(filePath));

    QFile file(QString::fromStdString(filePath));
    if (!file.exists()) {
        avLog("错误：文件不存在");
        if (callback) callback(false, gocook::models::AvatarUploadResponse{}, "文件不存在");
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        avLog("错误：无法打开文件");
        if (callback) callback(false, gocook::models::AvatarUploadResponse{}, "无法打开文件");
        return;
    }
    QByteArray fileData = file.readAll();
    QString fileName = QFileInfo(file.fileName()).fileName();
    file.close();
    avLog("文件名：" + fileName + "，大小：" + QString::number(fileData.size()) + " 字节");

    if (fileData.size() > 5 * 1024 * 1024) {
        avLog("错误：文件超过 5MB 上限");
        if (callback) callback(false, gocook::models::AvatarUploadResponse{}, "图片大小不能超过5MB");
        return;
    }

    // ★ 直接发原始二进制 POST，不经过 sendRequest（避免 JSON 序列化大数据的风险）
    QString lower = fileName.toLower();
    QString contentType = "image/jpeg";
    if (lower.endsWith(".png"))
        contentType = "image/png";
    else if (lower.endsWith(".gif"))
        contentType = "image/gif";
    else if (lower.endsWith(".bmp"))
        contentType = "image/bmp";
    else if (lower.endsWith(".webp"))
        contentType = "image/webp";
    else if (lower.endsWith(".svg") || lower.endsWith(".svgz"))
        contentType = "image/svg+xml";
    avLog("Content-Type：" + contentType + "，是否携带令牌：" + (m_token.isEmpty() ? "否" : "是"));

    QUrl url(m_baseUrl + "/api/users/me/avatar");
    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (!m_token.isEmpty())
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_token).toUtf8());
    avLog("发送 POST 请求：" + url.toString());

    sendRaw(AuthMode::Interactive, QNetworkAccessManager::PostOperation, request, fileData, "",
            [callback, avLog](int statusCode, const QByteArray &responseData) {
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        avLog("响应状态码：" + QString::number(statusCode));
        avLog("响应内容：" + QString::fromUtf8(responseData));

        if (statusCode == 401) {
            avLog("未授权(401)");
            if (callback) callback(false, gocook::models::AvatarUploadResponse{}, errorMessageFor(statusCode, doc).toStdString());
            return;
        }

        bool success = (statusCode >= 200 && statusCode < 300);
        if (!success) {
            // 统一三级文案：断网(statusCode=0)时不再把空 body 当错误信息抛给上层
            const QString err = errorMessageFor(statusCode, doc);
            avLog("请求失败：" + err);
            if (callback) callback(false, gocook::models::AvatarUploadResponse{}, err.toStdString());
            return;
        }

        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            gocook::models::AvatarUploadResponse resp;
            resp.avatar_id = obj["avatar_id"].toInt();
            resp.avatar_url = obj["avatar_url"].toString().toStdString();
            avLog("成功：avatar_id=" + QString::number(resp.avatar_id) + "，avatar_url=" + QString::fromStdString(resp.avatar_url));
            if (callback) callback(true, resp, "");
        } else {
            avLog("响应格式无效(非 JSON)");
            if (callback) callback(false, gocook::models::AvatarUploadResponse{}, "无效的响应格式");
        }
    });
}

void HttpGoCookApi::uploadRecipeImage(int recipeId,
                                      const std::string& filePath,
                                      RecipeImageCallback callback)
{
    QFile file(QString::fromStdString(filePath));
    if (!file.exists()) {
        if (callback) callback(false, "", "文件不存在");
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (callback) callback(false, "", "无法打开文件");
        return;
    }
    QByteArray fileData = file.readAll();
    QString fileName = QFileInfo(file.fileName()).fileName();
    file.close();

    if (fileData.size() > 5 * 1024 * 1024) {
        if (callback) callback(false, "", "图片大小不能超过5MB");
        return;
    }

    QString lower = fileName.toLower();
    QString contentType = "image/jpeg";
    if (lower.endsWith(".png"))        contentType = "image/png";
    else if (lower.endsWith(".gif"))   contentType = "image/gif";
    else if (lower.endsWith(".bmp"))   contentType = "image/bmp";
    else if (lower.endsWith(".webp"))  contentType = "image/webp";
    else if (lower.endsWith(".svg") || lower.endsWith(".svgz"))
        contentType = "image/svg+xml";

    QUrl url(m_baseUrl + QString("/api/recipes/%1/image").arg(recipeId));
    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (!m_token.isEmpty())
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_token).toUtf8());

    sendRaw(AuthMode::Interactive, QNetworkAccessManager::PostOperation, request, fileData, "",
            [callback](int statusCode, const QByteArray &responseData) {
        QJsonDocument doc = QJsonDocument::fromJson(responseData);

        if (statusCode == 401) {
            if (callback) callback(false, "", errorMessageFor(statusCode, doc).toStdString());
            return;
        }

        bool success = (statusCode >= 200 && statusCode < 300);
        if (!success) {
            // 统一三级文案：断网(statusCode=0)时不再把空 body 当错误信息抛给上层
            const QString err = errorMessageFor(statusCode, doc);
            if (callback) callback(false, "", err.toStdString());
            return;
        }

        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString imageUrl = obj["image_url"].toString();
            if (callback) callback(true, imageUrl.toStdString(), "");
        } else {
            if (callback) callback(false, "", "无效的响应格式");
        }
    });
}

void HttpGoCookApi::uploadStepImage(int recipeId, int stepIndex,
                                    const std::string& filePath,
                                    RecipeImageCallback callback)
{
    auto siLog = [](const QString& msg) {
        QFile f("/tmp/gocook_stepimage_debug.log");
        if (f.open(QIODevice::Append | QIODevice::Text)) {
            f.write(("[CLIENT] " + msg + "\n").toUtf8());
            f.close();
        }
    };
    siLog("=== 步骤图上传开始 ===");
    siLog("菜谱 ID=" + QString::number(recipeId) + "，步骤序号=" + QString::number(stepIndex)
          + "，文件路径=" + QString::fromStdString(filePath));

    QFile file(QString::fromStdString(filePath));
    if (!file.exists()) {
        siLog("错误：文件不存在");
        if (callback) callback(false, "", "文件不存在");
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        siLog("错误：无法打开文件");
        if (callback) callback(false, "", "无法打开文件");
        return;
    }
    QByteArray fileData = file.readAll();
    QString fileName = QFileInfo(file.fileName()).fileName();
    file.close();
    siLog("文件名=" + fileName + "，大小=" + QString::number(fileData.size()) + " 字节");

    if (fileData.size() > 5 * 1024 * 1024) {
        siLog("错误：文件超过 5MB 上限");
        if (callback) callback(false, "", "图片大小不能超过5MB");
        return;
    }

    QString lower = fileName.toLower();
    QString contentType = "image/jpeg";
    if (lower.endsWith(".png"))        contentType = "image/png";
    else if (lower.endsWith(".gif"))   contentType = "image/gif";
    else if (lower.endsWith(".bmp"))   contentType = "image/bmp";
    else if (lower.endsWith(".webp"))  contentType = "image/webp";
    else if (lower.endsWith(".svg") || lower.endsWith(".svgz"))
        contentType = "image/svg+xml";

    QUrl url(m_baseUrl + QString("/api/recipes/%1/steps/%2/image").arg(recipeId).arg(stepIndex));
    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (!m_token.isEmpty())
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_token).toUtf8());
    siLog("发送 POST 请求：" + url.toString() + "，Content-Type=" + contentType + "，是否携带令牌：" + (m_token.isEmpty() ? "否" : "是"));

    sendRaw(AuthMode::Interactive, QNetworkAccessManager::PostOperation, request, fileData, "",
            [callback, siLog](int statusCode, const QByteArray &responseData) {
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        siLog("响应状态码=" + QString::number(statusCode) + "，响应内容=" + QString::fromUtf8(responseData));

        if (statusCode == 401) {
            siLog("未授权(401)");
            if (callback) callback(false, "", errorMessageFor(statusCode, doc).toStdString());
            return;
        }

        bool success = (statusCode >= 200 && statusCode < 300);
        if (!success) {
            // 统一三级文案：断网(statusCode=0)时不再把空 body 当错误信息抛给上层
            const QString err = errorMessageFor(statusCode, doc);
            siLog("请求失败：" + err);
            if (callback) callback(false, "", err.toStdString());
            return;
        }

        std::string imageUrl;
        if (doc.isObject() && doc.object().contains("image_url"))
            imageUrl = doc.object()["image_url"].toString().toStdString();
        siLog("成功：image_url=" + QString::fromStdString(imageUrl));
        if (callback) callback(true, imageUrl, "");
    });
}

void HttpGoCookApi::deleteRecipe(int recipeId, SuccessCallback callback)
{
    deleteResource(QString("/api/recipes/%1").arg(recipeId), {},
        [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
            if (!success) {
                if (callback) callback(false, errorMsg.toStdString());
                return;
            }
            if (callback) callback(true, "");
        }, true);
}

void HttpGoCookApi::changePassword(const std::string& currentPassword,
                                   const std::string& newPassword,
                                   SuccessCallback callback)
{
    QVariantMap data;
    data["current_password"] = QString::fromStdString(currentPassword);
    data["new_password"] = QString::fromStdString(newPassword);

    put("/api/users/me/password", data, [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            QString err = errorStr;
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error"))
                    err = obj["error"].toString();
            }
            if (callback) callback(false, err.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::deleteAccount(SuccessCallback callback)
{
    deleteResource("/api/users/me", {}, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            QString err = errorMsg;
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error"))
                    err = obj["error"].toString();
            }
            if (callback) callback(false, err.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::getFavorites(int page, int size,
                                 const std::string& group,
                                 FavoritesCallback callback) {
    QString path = QString("/api/users/me/favorites?page=%1&size=%2").arg(page).arg(size);
    if (!group.empty())
        path += "&group=" + QString::fromStdString(group);
    get(path, [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::PagedFavorites{}, errorStr.toStdString());
            return;
        }
        gocook::models::PagedFavorites result;
        QJsonObject obj = doc.object();
        if (obj.contains("pagination")) {
            QJsonObject p = obj["pagination"].toObject();
            result.pagination.page = p["page"].toInt();
            result.pagination.size = p["size"].toInt();
            result.pagination.total = p["total"].toInt();
            result.pagination.total_pages = p["total_pages"].toInt();
        }
        if (obj.contains("data")) {
            for (const auto& v : obj["data"].toArray()) {
                QJsonObject item = v.toObject();
                gocook::models::FavoriteItem fi;
                fi.id = item["id"].toInt();
                fi.recipe_id = item["recipe_id"].toInt();
                fi.name = item["name"].toString().toStdString();
                fi.description = item["description"].toString().toStdString();
                fi.image_url = item["image_url"].toString().toStdString();
                fi.group_name = item["group_name"].toString().toStdString();
                fi.is_public = item["is_public"].toBool();
                fi.favorited_at = item["favorited_at"].toString().toStdString();
                result.data.push_back(fi);
            }
        }
        if (callback) callback(true, result, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::getFavoriteGroups(FavoriteGroupsCallback callback) {
    get("/api/users/me/favorites/groups", [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, std::vector<gocook::models::FavoriteGroup>{}, errorStr.toStdString());
            return;
        }
        std::vector<gocook::models::FavoriteGroup> groups;
        QJsonArray arr = doc.array();
        for (const auto& v : arr) {
            QJsonObject item = v.toObject();
            gocook::models::FavoriteGroup g;
            g.id = item["id"].toInt();
            g.name = item["name"].toString().toStdString();
            g.sort_order = item["sort_order"].toInt();
            g.count = item["count"].toInt();
            groups.push_back(g);
        }
        if (callback) callback(true, groups, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::createFavoriteGroup(const gocook::models::CreateGroupRequest& request,
                                        FavoriteGroupCallback callback) {
    QVariantMap data;
    data["name"] = QString::fromStdString(request.name);
    post("/api/users/me/favorites/groups", data, [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::FavoriteGroup{}, errorStr.toStdString());
            return;
        }
        QJsonObject obj = doc.object();
        gocook::models::FavoriteGroup group;
        group.id = obj["id"].toInt();
        group.name = obj["name"].toString().toStdString();
        group.sort_order = obj["sort_order"].toInt();
        group.count = obj["count"].toInt();
        if (callback) callback(true, group, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::updateFavoriteGroup(int groupId,
                                        const gocook::models::UpdateGroupRequest& request,
                                        SuccessCallback callback) {
    QVariantMap data;
    data["name"] = QString::fromStdString(request.name);
    put(QString("/api/users/me/favorites/groups/%1").arg(groupId), data,
        [callback](bool success, const QString& errorStr, const QJsonDocument&) {
        if (!success) {
            QString err = errorStr;
            if (callback) callback(false, err.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::deleteFavoriteGroup(int groupId,
                                        SuccessCallback callback) {
    deleteResource(QString("/api/users/me/favorites/groups/%1").arg(groupId), {},
        [callback](bool success, const QString& errorStr, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorStr.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::updateFavoriteItem(int favoriteId,
                                       const gocook::models::UpdateFavoriteRequest& request,
                                       SuccessCallback callback) {
    QVariantMap data;
    if (request.group_id.has_value())
        data["group_id"] = request.group_id.value();
    if (request.is_public.has_value())
        data["is_public"] = request.is_public.value();

    QUrl url(m_baseUrl + QString("/api/users/me/favorites/%1").arg(favoriteId));
    QNetworkRequest req(url);
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", QString("Bearer %1").arg(m_token).toUtf8());

    QByteArray body = QJsonDocument(QJsonObject::fromVariantMap(data)).toJson();
    sendRaw(AuthMode::Interactive, QNetworkAccessManager::CustomOperation, req, body, "PATCH",
            [callback](int statusCode, const QByteArray &responseData) {
        QJsonDocument doc = QJsonDocument::fromJson(responseData);

        // 401 全局 unauthorized 信号已由 sendRaw 统一发出（与其它接口一致：触发登出）
        if (statusCode == 401) {
            if (callback) callback(false, errorMessageFor(statusCode, doc).toStdString());
            return;
        }

        bool success = (statusCode >= 200 && statusCode < 300);
        if (callback) {
            if (success)
                callback(true, "");
            else
                // 统一三级文案：断网(statusCode=0)时不再把空 body 当错误信息抛给上层
                callback(false, errorMessageFor(statusCode, doc).toStdString());
        }
    });
}

void HttpGoCookApi::batchDeleteFavorites(const gocook::models::BatchDeleteFavoritesRequest& request,
                                         SuccessCallback callback) {
    QVariantMap data;
    QVariantList ids;
    for (const auto& fid : request.favorite_ids)
        ids.append(fid);
    data["favorite_ids"] = ids;
    // 使用 POST 而非 DELETE，httplib 对 POST body 支持更可靠
    post(QStringLiteral("/api/users/me/favorites/batch"), data,
        [callback](bool success, const QString& errorStr, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorStr.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::getNotifications(int page, int size,
                                     const std::string& type,
                                     PagedNotificationsCallback callback)
{
    QString path = QString("/api/users/me/notifications?page=%1&size=%2").arg(page).arg(size);
    if (!type.empty())
        path += "&type=" + QString::fromStdString(type);
    get(path, [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::PagedNotifications{}, errorStr.toStdString());
            return;
        }
        gocook::models::PagedNotifications result;
        QJsonObject obj = doc.object();
        if (obj.contains("pagination")) {
            QJsonObject p = obj["pagination"].toObject();
            result.pagination.page = p["page"].toInt();
            result.pagination.size = p["size"].toInt();
            result.pagination.total = p["total"].toInt();
            result.pagination.total_pages = p["total_pages"].toInt();
        }
        if (obj.contains("data")) {
            for (const auto& v : obj["data"].toArray()) {
                QJsonObject item = v.toObject();
                gocook::models::NotificationItem ni;
                ni.id = item["id"].toInt();
                ni.title = item["title"].toString().toStdString();
                ni.content = item["content"].toString().toStdString();
                ni.type = item["type"].toString().toStdString();
                if (item.contains("sub_type") && !item["sub_type"].isNull())
                    ni.sub_type = item["sub_type"].toString().toStdString();
                ni.is_read = item["is_read"].toBool();
                if (item.contains("related_id") && !item["related_id"].isNull())
                    ni.related_id = item["related_id"].toInt();
                if (item.contains("trigger_user_name") && !item["trigger_user_name"].isNull())
                    ni.trigger_user_name = item["trigger_user_name"].toString().toStdString();
                ni.created_at = item["created_at"].toString().toStdString();
                result.data.push_back(ni);
            }
        }
        if (callback) callback(true, result, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::markNotificationRead(int notificationId,
                                         SuccessCallback callback)
{
    patch(QString("/api/users/me/notifications/%1/read").arg(notificationId), {},
        [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            QString err = errorStr;
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error"))
                    err = obj["error"].toString();
            }
            if (callback) callback(false, err.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::markAllNotificationsRead(SuccessCallback callback)
{
    put("/api/users/me/notifications/read-all", {},
        [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            QString err = errorStr;
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error"))
                    err = obj["error"].toString();
            }
            if (callback) callback(false, err.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::deleteNotification(int notificationId,
                                        SuccessCallback callback)
{
    deleteResource(QString("/api/users/me/notifications/%1").arg(notificationId), {},
        [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            QString err = errorStr;
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error"))
                    err = obj["error"].toString();
            }
            if (callback) callback(false, err.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

// ======================= 菜谱相关 =======================

// 辅助函数：将 QJsonObject 解析为 RecipeSummary
static gocook::models::RecipeSummary parseRecipeSummary(const QJsonObject &obj)
{
    gocook::models::RecipeSummary recipe;
    recipe.id                  = obj["id"].toInt();
    recipe.name                = obj["name"].toString().toStdString();
    recipe.description         = obj["description"].toString().toStdString();
    recipe.image_url           = obj["image_url"].toString().toStdString();
    recipe.prep_time_minutes   = obj["prep_time_minutes"].toInt();
    recipe.cook_time_minutes   = obj["cook_time_minutes"].toInt();
    recipe.author_id           = obj["author_id"].toInt();
    recipe.author_name         = obj["author_name"].toString().toStdString();

    // 解析 tags 数组
    if (obj.contains("tags") && obj["tags"].isArray()) {
        const QJsonArray tagsArr = obj["tags"].toArray();
        for (const auto &tag : tagsArr)
            recipe.tags.push_back(tag.toString().toStdString());
    }
    return recipe;
}

void HttpGoCookApi::getPublicRecipes(int page, int size,
                                     const nlohmann::json &filters,
                                     PagedRecipesCallback callback)
{
    QVariantMap params;
    params["page"] = page;
    params["size"] = size;

    if (!filters.is_null()) {
        if (filters.contains("cuisine"))
            params["cuisine"] = QString::fromStdString(filters["cuisine"].get<std::string>());
        if (filters.contains("meal_type"))
            params["meal_type"] = QString::fromStdString(filters["meal_type"].get<std::string>());
        if (filters.contains("difficulty"))
            params["difficulty"] = QString::fromStdString(filters["difficulty"].get<std::string>());
        if (filters.contains("max_time"))
            params["max_time"] = filters["max_time"].get<int>();
        if (filters.contains("tags")) {
            QStringList tagList;
            for (const auto &tag : filters["tags"])
                tagList << QString::fromStdString(tag.get<std::string>());
            if (!tagList.isEmpty())
                params["tags"] = tagList.join(",");
        }
    }

    QUrlQuery query;
    for (auto it = params.begin(); it != params.end(); ++it)
        query.addQueryItem(it.key(), it.value().toString());
    QString endpoint = "/api/recipes/public";
    if (!query.isEmpty())
        endpoint += "?" + query.toString(QUrl::FullyEncoded);

    get(endpoint, [callback](bool success, const QString &errorMsg, const QJsonDocument &doc) {
        if (!success) {
            callback(false, gocook::models::PagedRecipes{},
                     errorMsg.isEmpty() ? "未知错误" : errorMsg.toStdString());
            return;
        }
        QJsonObject root = doc.object();
        gocook::models::PagedRecipes result;
        if (root.contains("pagination") && root["pagination"].isObject()) {
            QJsonObject pag = root["pagination"].toObject();
            result.pagination.page        = pag["page"].toInt();
            result.pagination.size        = pag["size"].toInt();
            result.pagination.total       = pag["total"].toInt();
            result.pagination.total_pages = pag["total_pages"].toInt();
        }
        if (root.contains("data") && root["data"].isArray()) {
            const QJsonArray dataArr = root["data"].toArray();
            for (const QJsonValue &val : dataArr)
                result.data.push_back(parseRecipeSummary(val.toObject()));
        }
        callback(true, result, "");
    }, true);
}

void HttpGoCookApi::getRecommendedRecipes(int page, int size,
                                          PagedRecommendedRecipesCallback callback) {
    QUrlQuery query;
    query.addQueryItem("page", QString::number(page));
    query.addQueryItem("size", QString::number(size));
    QString endpoint = QString("/api/recipes/recommend?%1").arg(query.toString(QUrl::FullyEncoded));

    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            callback(false, gocook::models::PagedRecommendedRecipes{},
                     errorMsg.isEmpty() ? "未知错误" : errorMsg.toStdString());
            return;
        }
        QJsonObject root = doc.object();
        gocook::models::PagedRecommendedRecipes result;

        // health_filter_applied
        result.health_filter_applied = root["health_filter_applied"].toBool(false);

        // 分页信息
        if (root.contains("pagination") && root["pagination"].isObject()) {
            QJsonObject pag = root["pagination"].toObject();
            result.pagination.page        = pag["page"].toInt();
            result.pagination.size        = pag["size"].toInt();
            result.pagination.total       = pag["total"].toInt();
            result.pagination.total_pages = pag["total_pages"].toInt();
        }

        // data 数组
        if (root.contains("data") && root["data"].isArray()) {
            const QJsonArray dataArr = root["data"].toArray();
            for (const QJsonValue& val : dataArr) {
                QJsonObject obj = val.toObject();
                gocook::models::RecommendedRecipe rec;

                // RecipeSummary 公共字段
                rec.id                = obj["id"].toInt();
                rec.name              = obj["name"].toString().toStdString();
                rec.description       = obj["description"].toString().toStdString();
                rec.image_url         = obj["image_url"].toString().toStdString();
                rec.prep_time_minutes = obj["prep_time_minutes"].toInt();
                rec.cook_time_minutes = obj["cook_time_minutes"].toInt();
                rec.calories          = obj["calories"].toInt();
                rec.view_count        = obj["view_count"].toInt();
                rec.avg_rating        = obj["avg_rating"].toDouble();
                rec.author_id         = obj["author_id"].toInt();
                rec.author_name       = obj["author_name"].toString().toStdString();
                rec.cooking_method    = obj["cooking_method"].toString().toStdString();
                rec.flavor            = obj["flavor"].toString().toStdString();
                rec.ingredient_type   = obj["ingredient_type"].toString().toStdString();

                if (obj.contains("tags") && obj["tags"].isArray()) {
                    const QJsonArray tagsArr = obj["tags"].toArray();
                    for (const auto& t : tagsArr)
                        rec.tags.push_back(t.toString().toStdString());
                }

                // RecommendedRecipe 扩展字段
                rec.match_score = obj["match_score"].toDouble();
                rec.health_notice = obj["health_notice"].toString().toStdString();

                QJsonObject status = obj["match_status"].toObject();
                if (status.contains("available_ingredients") && status["available_ingredients"].isArray()) {
                    const QJsonArray arr = status["available_ingredients"].toArray();
                    for (const auto& v : arr) {
                        QJsonObject ing = v.toObject();
                        gocook::models::MatchIngredient mi;
                        mi.name     = ing["name"].toString().toStdString();
                        mi.quantity = ing["quantity"].toDouble();
                        mi.unit     = ing["unit"].toString().toStdString();
                        rec.match_status.available_ingredients.push_back(std::move(mi));
                    }
                }
                if (status.contains("missing_ingredients") && status["missing_ingredients"].isArray()) {
                    const QJsonArray arr = status["missing_ingredients"].toArray();
                    for (const auto& v : arr) {
                        QJsonObject ing = v.toObject();
                        gocook::models::MissingIngredient mi;
                        mi.name     = ing["name"].toString().toStdString();
                        mi.quantity = ing["quantity"].toDouble();
                        mi.unit     = ing["unit"].toString().toStdString();
                        rec.match_status.missing_ingredients.push_back(std::move(mi));
                    }
                }

                result.data.push_back(std::move(rec));
            }
        }

        callback(true, result, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::searchRecipes(const std::string& keyword,
                                  int page, int size,
                                  const nlohmann::json& filters,
                                  PagedRecipesCallback callback) {
    QVariantMap params;
    params["keyword"] = QString::fromStdString(keyword);
    params["page"] = page;
    params["size"] = size;

    if (!filters.is_null()) {
        if (filters.contains("cuisine"))
            params["cuisine"] = QString::fromStdString(filters["cuisine"].get<std::string>());
        if (filters.contains("meal_type"))
            params["meal_type"] = QString::fromStdString(filters["meal_type"].get<std::string>());
        if (filters.contains("difficulty"))
            params["difficulty"] = QString::fromStdString(filters["difficulty"].get<std::string>());
        if (filters.contains("flavor"))
            params["flavor"] = QString::fromStdString(filters["flavor"].get<std::string>());
        if (filters.contains("cooking_method"))
            params["cooking_method"] = QString::fromStdString(filters["cooking_method"].get<std::string>());
        if (filters.contains("ingredient_type"))
            params["ingredient_type"] = QString::fromStdString(filters["ingredient_type"].get<std::string>());
        if (filters.contains("max_time"))
            params["max_time"] = filters["max_time"].get<int>();
        if (filters.contains("min_calories"))
            params["min_calories"] = filters["min_calories"].get<int>();
        if (filters.contains("max_calories"))
            params["max_calories"] = filters["max_calories"].get<int>();
        if (filters.contains("min_rating"))
            params["min_rating"] = filters["min_rating"].get<double>();
        if (filters.contains("sort_by"))
            params["sort_by"] = QString::fromStdString(filters["sort_by"].get<std::string>());
        if (filters.contains("tags")) {
            QStringList tagList;
            for (const auto& tag : filters["tags"])
                tagList << QString::fromStdString(tag.get<std::string>());
            if (!tagList.isEmpty())
                params["tags"] = tagList.join(",");
        }
    }

    QUrlQuery query;
    for (auto it = params.begin(); it != params.end(); ++it)
        query.addQueryItem(it.key(), it.value().toString());
    QString endpoint = "/api/recipes/search";
    if (!query.isEmpty())
        endpoint += "?" + query.toString(QUrl::FullyEncoded);

    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            callback(false, gocook::models::PagedRecipes{},
                     errorMsg.isEmpty() ? "未知错误" : errorMsg.toStdString());
            return;
        }
        QJsonObject root = doc.object();
        gocook::models::PagedRecipes result;
        if (root.contains("pagination") && root["pagination"].isObject()) {
            QJsonObject pag = root["pagination"].toObject();
            result.pagination.page        = pag["page"].toInt();
            result.pagination.size        = pag["size"].toInt();
            result.pagination.total       = pag["total"].toInt();
            result.pagination.total_pages = pag["total_pages"].toInt();
        }
        if (root.contains("data") && root["data"].isArray()) {
            const QJsonArray dataArr = root["data"].toArray();
            for (const QJsonValue& val : dataArr)
                result.data.push_back(parseRecipeSummary(val.toObject()));
        }
        callback(true, result, "");
    }, true);
}

void HttpGoCookApi::getRecipeDetail(int recipeId,
                                    RecipeDetailCallback callback) {
    QString endpoint = QString("/api/recipes/%1").arg(recipeId);
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            callback(false, gocook::models::RecipeDetail{}, errorMsg.toStdString());
            return;
        }
        QJsonObject obj = doc.object();
        gocook::models::RecipeDetail detail;
        detail.id = obj["id"].toInt();
        detail.name = obj["name"].toString().toStdString();
        detail.description = obj["description"].toString().toStdString();
        detail.image_url = obj["image_url"].toString().toStdString();
        detail.cooking_method = obj["cooking_method"].toString().toStdString();
        detail.flavor = obj["flavor"].toString().toStdString();
        detail.prep_time_minutes = obj["prep_time_minutes"].toInt();
        detail.cook_time_minutes = obj["cook_time_minutes"].toInt();
        detail.view_count = obj["view_count"].toInt();
        detail.avg_rating = obj["avg_rating"].toDouble();
        if (obj.contains("ingredients") && obj["ingredients"].isArray()) {
            for (const auto& val : obj["ingredients"].toArray()) {
                QJsonObject ingObj = val.toObject();
                gocook::models::Ingredient ing;
                ing.name = ingObj["name"].toString().toStdString();
                ing.quantity = ingObj["quantity"].toDouble();
                ing.unit = ingObj["unit"].toString().toStdString();
                detail.ingredients.push_back(ing);
            }
        }
        if (obj.contains("steps") && obj["steps"].isArray()) {
            for (const auto& val : obj["steps"].toArray()) {
                QJsonObject stepObj = val.toObject();
                gocook::models::CookingStep step;
                step.order = stepObj["order"].toInt();
                step.description = stepObj["description"].toString().toStdString();
                if (stepObj.contains("duration"))
                    step.duration = stepObj["duration"].toInt();
                if (stepObj.contains("image_url"))
                    step.image_url = stepObj["image_url"].toString().toStdString();
                detail.steps.push_back(step);
            }
        }
        if (obj.contains("nutrition") && obj["nutrition"].isObject()) {
            QJsonObject nut = obj["nutrition"].toObject();
            detail.nutrition.calories = nut["calories"].toDouble();
            detail.nutrition.protein = nut["protein"].toDouble();
            detail.nutrition.fat = nut["fat"].toDouble();
            detail.nutrition.carbs = nut["carbs"].toDouble();
            if (nut.contains("has_data")) {
                // 新服务端：has_data 为权威判定（false=无营养报告，前端展示空态而非全 0）
                detail.nutrition.has_data = nut["has_data"].toBool();
            } else {
                // 旧服务端未返回该字段：按四项值兜底（等价于修复前 QML 的 calories>0 判定，
                // 避免无数据菜谱的"营养合计"卡片显示全 0）
                detail.nutrition.has_data = detail.nutrition.calories > 0
                    || detail.nutrition.protein > 0
                    || detail.nutrition.fat > 0
                    || detail.nutrition.carbs > 0;
            }
        }
        if (obj.contains("tags") && obj["tags"].isArray()) {
            for (const auto& val : obj["tags"].toArray())
                detail.tags.push_back(val.toString().toStdString());
        }
        detail.author_id = obj["author_id"].toInt();
        detail.author_name = obj["author_name"].toString().toStdString();
        detail.is_favorited = obj["is_favorited"].toBool();
        detail.created_at = obj["created_at"].toString().toStdString();
        detail.updated_at = obj["updated_at"].toString().toStdString();
        callback(true, detail, "");
    }, true);
}

void HttpGoCookApi::getRecipeVideos(int recipeId,
                                    RecipeVideosCallback callback) {
    QString endpoint = QString("/api/recipes/%1/videos").arg(recipeId);
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            callback(false, {}, errorMsg.toStdString());
            return;
        }
        QJsonArray arr = doc.array();
        std::vector<gocook::models::RecipeVideo> videos;
        for (const auto& val : arr) {
            QJsonObject obj = val.toObject();
            gocook::models::RecipeVideo v;
            v.id = obj["id"].toInt();
            v.title = obj["title"].toString().toStdString();
            v.platform = obj["platform"].toString().toStdString();
            v.url = obj["url"].toString().toStdString();
            v.thumbnail_url = obj["thumbnail_url"].toString().toStdString();
            v.duration_seconds = obj["duration_seconds"].toInt();
            videos.push_back(std::move(v));
        }
        callback(true, videos, "");
    }, true);
}

void HttpGoCookApi::getRecipeRatings(int recipeId, int page, int size,
                                     PagedRatingsCallback callback) {
    QString endpoint = QString("/api/recipes/%1/ratings?page=%2&size=%3")
                           .arg(recipeId).arg(page).arg(size);
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            callback(false, gocook::models::PagedRatings{}, errorMsg.toStdString());
            return;
        }
        QJsonObject root = doc.object();
        QJsonArray arr = root["data"].toArray();
        QJsonObject pag = root["pagination"].toObject();

        gocook::models::PagedRatings result;
        for (const auto& val : arr) {
            QJsonObject obj = val.toObject();
            gocook::models::RecipeRating r;
            r.id = obj["id"].toInt();
            r.user_id = obj["user_id"].toInt();
            r.username = obj["username"].toString().toStdString();
            r.rating = obj["rating"].toInt();
            r.comment = obj["comment"].toString().toStdString();
            r.created_at = obj["created_at"].toString().toStdString();
            result.data.push_back(std::move(r));
        }
        result.pagination.page = pag["page"].toInt();
        result.pagination.size = pag["size"].toInt();
        result.pagination.total = pag["total"].toInt();
        result.pagination.total_pages = pag["total_pages"].toInt();

        callback(true, result, "");
    }, true);
}

void HttpGoCookApi::getMyRecipeRating(int recipeId,
                                       MyRecipeRatingCallback callback) {
    QString endpoint = QString("/api/recipes/%1/ratings/mine").arg(recipeId);
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            callback(false, std::nullopt, errorMsg.toStdString());
            return;
        }
        QJsonObject obj = doc.object();
        if (obj.isEmpty()) {
            callback(true, std::nullopt, "");
            return;
        }
        gocook::models::RecipeRating r;
        r.id = obj["id"].toInt();
        r.user_id = obj["user_id"].toInt();
        r.username = obj["username"].toString().toStdString();
        r.rating = obj["rating"].toInt();
        r.comment = obj["comment"].toString().toStdString();
        r.created_at = obj["created_at"].toString().toStdString();
        callback(true, std::move(r), "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::submitRecipe(const gocook::models::SubmitRecipeRequest& recipeData,
                                 SubmitRecipeCallback callback) {
    QVariantMap data;
    data["name"] = QString::fromStdString(recipeData.name);
    data["description"] = QString::fromStdString(recipeData.description);
    data["image_url"] = QString::fromStdString(recipeData.image_url);

    QVariantList ingredients;
    for (const auto& ing : recipeData.ingredients) {
        QVariantMap ingMap;
        ingMap["name"] = QString::fromStdString(ing.name);
        ingMap["quantity"] = ing.quantity;
        ingMap["unit"] = QString::fromStdString(ing.unit);
        ingredients.append(ingMap);
    }
    data["ingredients"] = ingredients;

    QVariantList steps;
    for (const auto& s : recipeData.steps) {
        QVariantMap stepMap;
        stepMap["order"] = s.order;
        stepMap["description"] = QString::fromStdString(s.description);
        if (s.duration.has_value())
            stepMap["duration"] = s.duration.value();
        if (!s.image_url.empty())
            stepMap["image_url"] = QString::fromStdString(s.image_url);
        steps.append(stepMap);
    }
    data["steps"] = steps;

    if (recipeData.nutrition.has_value()) {
        QVariantMap nutMap;
        nutMap["calories"] = recipeData.nutrition->calories;
        nutMap["protein"] = recipeData.nutrition->protein;
        nutMap["fat"] = recipeData.nutrition->fat;
        nutMap["carbs"] = recipeData.nutrition->carbs;
        data["nutrition"] = nutMap;
    }

    if (!recipeData.tags.empty()) {
        QStringList tagList;
        for (const auto& t : recipeData.tags)
            tagList << QString::fromStdString(t);
        data["tags"] = tagList;
    }

    if (recipeData.cooking_method.has_value())
        data["cooking_method"] = QString::fromStdString(recipeData.cooking_method.value());
    if (recipeData.flavor.has_value())
        data["flavor"] = QString::fromStdString(recipeData.flavor.value());
    if (recipeData.ingredient_type.has_value())
        data["ingredient_type"] = QString::fromStdString(recipeData.ingredient_type.value());

    post("/api/recipes", data, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            callback(false, gocook::models::SubmitRecipeResponse{}, errorMsg.toStdString());
            return;
        }
        QJsonObject obj = doc.object();
        gocook::models::SubmitRecipeResponse resp;
        resp.id = obj["id"].toInt();
        resp.status = obj["status"].toString().toStdString();
        callback(true, resp, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::getMySubmittedRecipes(int page, int size,
                                          const std::string& status,
                                          PagedMyRecipesCallback callback) {
    QVariantMap params;
    params["page"] = page;
    params["size"] = size;
    if (!status.empty())
        params["status"] = QString::fromStdString(status);

    QUrlQuery query;
    for (auto it = params.begin(); it != params.end(); ++it)
        query.addQueryItem(it.key(), it.value().toString());
    QString endpoint = "/api/recipes/my";
    if (!query.isEmpty())
        endpoint += "?" + query.toString(QUrl::FullyEncoded);

    get(endpoint, [callback](bool success, const QString &errorMsg, const QJsonDocument &doc) {
        if (!success) {
            callback(false, gocook::models::PagedMyRecipes{},
                     errorMsg.isEmpty() ? "未知错误" : errorMsg.toStdString());
            return;
        }
        QJsonObject root = doc.object();
        gocook::models::PagedMyRecipes result;
        if (root.contains("pagination") && root["pagination"].isObject()) {
            QJsonObject pag = root["pagination"].toObject();
            result.pagination.page        = pag["page"].toInt();
            result.pagination.size        = pag["size"].toInt();
            result.pagination.total       = pag["total"].toInt();
            result.pagination.total_pages = pag["total_pages"].toInt();
        }
        if (root.contains("data") && root["data"].isArray()) {
            const QJsonArray dataArr = root["data"].toArray();
            for (const QJsonValue &val : dataArr) {
                QJsonObject obj = val.toObject();
                gocook::models::MyRecipeStatus item;
                item.id           = obj["id"].toInt();
                item.name         = obj["name"].toString().toStdString();
                item.status       = obj["status"].toString().toStdString();
                if (obj.contains("reject_reason") && !obj["reject_reason"].isNull())
                    item.reject_reason = obj["reject_reason"].toString().toStdString();
                item.submitted_at = obj["submitted_at"].toString().toStdString();
                item.updated_at   = obj["updated_at"].toString().toStdString();
                result.data.push_back(std::move(item));
            }
        }
        callback(true, result, "");
    }, false, AuthMode::Silent);
}

void HttpGoCookApi::editRecipe(int recipeId,
                               const gocook::models::EditRecipeRequest& updates,
                               SuccessCallback callback) {
    QVariantMap data;
    data["name"] = QString::fromStdString(updates.name);
    data["description"] = QString::fromStdString(updates.description);
    data["image_url"] = QString::fromStdString(updates.image_url);

    QVariantList ingredients;
    for (const auto& ing : updates.ingredients) {
        QVariantMap ingMap;
        ingMap["name"] = QString::fromStdString(ing.name);
        ingMap["quantity"] = ing.quantity;
        ingMap["unit"] = QString::fromStdString(ing.unit);
        ingredients.append(ingMap);
    }
    data["ingredients"] = ingredients;

    QVariantList steps;
    for (const auto& s : updates.steps) {
        QVariantMap stepMap;
        stepMap["order"] = s.order;
        stepMap["description"] = QString::fromStdString(s.description);
        if (s.duration.has_value())
            stepMap["duration"] = s.duration.value();
        if (!s.image_url.empty())
            stepMap["image_url"] = QString::fromStdString(s.image_url);
        steps.append(stepMap);
    }
    data["steps"] = steps;

    if (updates.nutrition.has_value()) {
        QVariantMap nutMap;
        nutMap["calories"] = updates.nutrition->calories;
        nutMap["protein"] = updates.nutrition->protein;
        nutMap["fat"] = updates.nutrition->fat;
        nutMap["carbs"] = updates.nutrition->carbs;
        data["nutrition"] = nutMap;
    }

    if (!updates.tags.empty()) {
        QStringList tagList;
        for (const auto& t : updates.tags)
            tagList << QString::fromStdString(t);
        data["tags"] = tagList;
    }

    if (updates.cooking_method.has_value())
        data["cooking_method"] = QString::fromStdString(updates.cooking_method.value());
    if (updates.flavor.has_value())
        data["flavor"] = QString::fromStdString(updates.flavor.value());
    if (updates.ingredient_type.has_value())
        data["ingredient_type"] = QString::fromStdString(updates.ingredient_type.value());

    QString endpoint = QString("/api/recipes/%1").arg(recipeId);
    put(endpoint, data, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (!success) {
            if (callback)
                callback(false, errorMsg.toStdString());
            return;
        }
        if (callback)
            callback(true, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::toggleFavorite(int recipeId,
                                   std::optional<int> groupId,
                                   std::optional<bool> isPublic,
                                   SuccessCallback callback) {
    QVariantMap data;
    if (groupId.has_value())
        data["group_id"] = groupId.value();
    if (isPublic.has_value())
        data["is_public"] = isPublic.value();
    post(QString("/api/recipes/%1/favorite").arg(recipeId), data,
        [callback](bool success, const QString& errorStr, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorStr.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::rateRecipe(int recipeId,
                               const gocook::models::RateRecipeRequest& request,
                               SuccessCallback callback) {
    QString endpoint = QString("/api/recipes/%1/rate").arg(recipeId);
    QVariantMap data;
    data["rating"] = request.rating;
    data["comment"] = QString::fromStdString(request.comment);
    post(endpoint, data, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (callback) callback(success, errorMsg.toStdString());
    }, false, AuthMode::Interactive);
}

void HttpGoCookApi::updateRating(int recipeId, int ratingId,
                                  const gocook::models::RateRecipeRequest& request,
                                  SuccessCallback callback)
{
    QString endpoint = QString("/api/recipes/%1/ratings/%2").arg(recipeId).arg(ratingId);
    QVariantMap data;
    data["rating"] = request.rating;
    data["comment"] = QString::fromStdString(request.comment);
    put(endpoint, data, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (callback) callback(success, errorMsg.toStdString());
    }, false, AuthMode::Interactive);
}

void HttpGoCookApi::deleteRating(int recipeId, int ratingId,
                                  SuccessCallback callback)
{
    QString endpoint = QString("/api/recipes/%1/ratings/%2").arg(recipeId).arg(ratingId);
    deleteResource(endpoint, QVariantMap{}, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (callback) callback(success, errorMsg.toStdString());
    }, false, AuthMode::Interactive);
}

void HttpGoCookApi::getMyRatings(int page, int size,
                                  PagedUserRatingsCallback callback)
{
    QVariantMap params;
    params["page"] = page;
    params["size"] = size;

    QUrlQuery query;
    for (auto it = params.begin(); it != params.end(); ++it)
        query.addQueryItem(it.key(), it.value().toString());
    QString endpoint = "/api/users/me/ratings";
    if (!query.isEmpty())
        endpoint += "?" + query.toString(QUrl::FullyEncoded);

    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::PagedUserRatings{},
                                   errorMsg.isEmpty() ? "未知错误" : errorMsg.toStdString());
            return;
        }
        QJsonObject root = doc.object();
        gocook::models::PagedUserRatings result;

        // 解析 pagination
        QJsonObject paginationObj = root["pagination"].toObject();
        result.pagination.page = paginationObj["page"].toInt(1);
        result.pagination.size = paginationObj["size"].toInt(20);
        result.pagination.total = paginationObj["total"].toInt(0);
        result.pagination.total_pages = paginationObj["total_pages"].toInt(0);

        // 解析 data 数组
        QJsonArray dataArr = root["data"].toArray();
        for (const auto& val : dataArr) {
            QJsonObject obj = val.toObject();
            gocook::models::UserRatingItem item;
            item.rating_id = obj["rating_id"].toInt();
            item.recipe_id = obj["recipe_id"].toInt();
            item.recipe_name = obj["recipe_name"].toString().toStdString();
            item.rating = obj["rating"].toInt();
            item.comment = obj["comment"].toString().toStdString();
            item.created_at = obj["created_at"].toString().toStdString();
            item.updated_at = obj["updated_at"].toString().toStdString();
            result.data.push_back(std::move(item));
        }

        if (callback) callback(true, result, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::getRecipeNutrition(int recipeId,
                                        NutritionReportCallback callback)
{
    QString endpoint = QString("/api/recipes/%1/nutrition").arg(recipeId);
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            callback(false, gocook::models::NutritionReport{}, errorMsg.toStdString());
            return;
        }
        QJsonObject obj = doc.object();
        gocook::models::NutritionReport report;
        report.recipe_id = obj["recipe_id"].toInt();
        report.recipe_name = obj["recipe_name"].toString().toStdString();

        QJsonObject ps = obj["per_serving"].toObject();
        report.per_serving.calories = ps["calories"].toDouble();
        report.per_serving.protein_g = ps["protein_g"].toDouble();
        report.per_serving.fat_g = ps["fat_g"].toDouble();
        report.per_serving.carbs_g = ps["carbs_g"].toDouble();
        report.per_serving.fiber_g = ps["fiber_g"].toDouble();
        report.per_serving.sodium_mg = ps["sodium_mg"].toDouble();
        report.per_serving.vitamin_c_mg = ps["vitamin_c_mg"].toDouble();

        QJsonArray breakdownArr = obj["ingredients_breakdown"].toArray();
        for (const QJsonValue& val : breakdownArr) {
            QJsonObject item = val.toObject();
            gocook::models::NutritionBreakdownItem bi;
            bi.name = item["name"].toString().toStdString();
            bi.calories = item["calories"].toDouble();
            bi.protein_g = item["protein_g"].toDouble();
            bi.fat_g = item["fat_g"].toDouble();
            bi.carbs_g = item["carbs_g"].toDouble();
            report.ingredients_breakdown.push_back(std::move(bi));
        }

        QJsonArray excludedArr = obj["excluded_ingredients"].toArray();
        for (const QJsonValue& val : excludedArr) {
            QJsonObject item = val.toObject();
            gocook::models::ExcludedIngredient ei;
            ei.name = item["name"].toString().toStdString();
            ei.reason = item["reason"].toString().toStdString();
            report.excluded_ingredients.push_back(std::move(ei));
        }

        report.health_notes = obj["health_notes"].toString().toStdString();
        if (obj.contains("has_data")) {
            // 新服务端：has_data 为权威判定（false=该菜谱暂无营养报告，展示空态）
            report.has_data = obj["has_data"].toBool();
        } else {
            // 旧服务端（v2.8-）无该字段：200 响应必有 per_serving 数据（无数据时返回 400），
            // 按 per_serving 存在性兜底；避免把"有数据的老响应"误判为空态
            report.has_data = obj.contains("per_serving") && obj["per_serving"].isObject();
        }

        callback(true, report, "");
    }, true);
}

// ======================= 库存管理 =======================
void HttpGoCookApi::getInventory(int page, int size,
                                 PagedInventoryCallback callback) {
    QUrlQuery query;
    query.addQueryItem("page", QString::number(page));
    query.addQueryItem("size", QString::number(size));
    QString endpoint = "/api/inventory?" + query.toString(QUrl::FullyEncoded);

    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            callback(false, gocook::models::PagedInventory{}, errorMsg.toStdString());
            return;
        }
        QJsonObject root = doc.object();
        gocook::models::PagedInventory result;
        if (root.contains("pagination") && root["pagination"].isObject()) {
            QJsonObject pag = root["pagination"].toObject();
            result.pagination.page = pag["page"].toInt();
            result.pagination.size = pag["size"].toInt();
            result.pagination.total = pag["total"].toInt();
            result.pagination.total_pages = pag["total_pages"].toInt();
        }
        if (root.contains("data") && root["data"].isArray()) {
            for (const auto& val : root["data"].toArray()) {
                QJsonObject obj = val.toObject();
                gocook::models::InventoryItem item;
                item.id = obj["id"].toInt();
                item.ingredient_name = obj["ingredient_name"].toString().toStdString();
                item.quantity = obj["quantity"].toDouble();
                item.unit = obj["unit"].toString().toStdString();
                if (obj.contains("expiry_date") && !obj["expiry_date"].isNull())
                    item.expiry_date = obj["expiry_date"].toString().toStdString();
                item.added_at = obj["added_at"].toString().toStdString();
                result.data.push_back(item);
            }
        }
        callback(true, result, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::upsertInventory(const gocook::models::UpsertInventoryRequest& item,
                                    IntCallback callback) {
    QVariantMap data;
    data["ingredient_name"] = QString::fromStdString(item.ingredient_name);
    data["quantity"] = item.quantity;
    data["unit"] = QString::fromStdString(item.unit);
    if (item.expiry_date.has_value())
        data["expiry_date"] = QString::fromStdString(item.expiry_date.value());

    post("/api/inventory", data, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            callback(false, 0, errorMsg.toStdString());
            return;
        }
        int id = doc.object()["id"].toInt();
        callback(true, id, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::updateInventoryItem(int itemId,
                                        const gocook::models::UpsertInventoryRequest& item,
                                        SuccessCallback callback) {
    QVariantMap data;
    data["ingredient_name"] = QString::fromStdString(item.ingredient_name);
    data["quantity"] = item.quantity;
    data["unit"] = QString::fromStdString(item.unit);
    if (item.expiry_date.has_value())
        data["expiry_date"] = QString::fromStdString(item.expiry_date.value());

    // 编辑 = 按 id 整行替换（PUT），与“添加=POST 累加”语义分离（决策 2026-09-04）
    put(QString("/api/inventory/%1").arg(itemId), data,
        [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
            callback(success, success ? "" : errorMsg.toStdString());
        }, true, AuthMode::Interactive);
}

void HttpGoCookApi::deleteInventoryItem(int itemId,
                                        SuccessCallback callback) {
    QString endpoint = QString("/api/inventory/%1").arg(itemId);
    deleteResource(endpoint, {}, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        callback(success, success ? "" : errorMsg.toStdString());
    }, true, AuthMode::Interactive);
}

// ======================= 购物清单 / 膳食计划 =======================
void HttpGoCookApi::getShoppingLists(ShoppingListsCallback callback)
{
    get("/api/inventory/shopping-lists", [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, std::vector<gocook::models::ShoppingListSummary>{}, errorMsg.toStdString());
            return;
        }
        std::vector<gocook::models::ShoppingListSummary> result;
        const QJsonArray arr = doc.array();
        for (const QJsonValue& val : arr) {
            QJsonObject obj = val.toObject();
            gocook::models::ShoppingListSummary summary;
            summary.id = obj["id"].toInt();
            summary.name = obj["name"].toString().toStdString();
            summary.item_count = obj["item_count"].toInt();
            summary.created_at = obj["created_at"].toString().toStdString();
            result.push_back(std::move(summary));
        }
        if (callback) callback(true, result, "");
    }, false, AuthMode::Silent);
}

void HttpGoCookApi::createShoppingList(const gocook::models::CreateShoppingListRequest& request,
                                        ShoppingListCallback callback)
{
    QVariantMap data;
    data["name"] = QString::fromStdString(request.name);
    if (request.plan_id.has_value())
        data["plan_id"] = QString::fromStdString(request.plan_id.value());

    post("/api/inventory/shopping-lists", data, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::ShoppingList{}, errorMsg.toStdString());
            return;
        }
        gocook::models::ShoppingList list;
        QJsonObject obj = doc.object();
        list.id = obj["id"].toInt();
        list.name = obj["name"].toString().toStdString();
        if (obj.contains("items") && obj["items"].isArray()) {
            const QJsonArray itemsArr = obj["items"].toArray();
            for (const QJsonValue& val : itemsArr) {
                QJsonObject itemObj = val.toObject();
                gocook::models::ShoppingListItem item;
                item.id = itemObj["id"].toInt();
                item.ingredient_name = itemObj["ingredient_name"].toString().toStdString();
                item.required_quantity = itemObj["required_quantity"].toDouble();
                item.inventory_quantity = itemObj["inventory_quantity"].toDouble();
                item.to_buy_quantity = itemObj["to_buy_quantity"].toDouble();
                item.unit = itemObj["unit"].toString().toStdString();
                item.checked = itemObj["checked"].toBool();
                list.items.push_back(std::move(item));
            }
        }
        if (callback) callback(true, list, "");
    }, false, AuthMode::Interactive);
}

void HttpGoCookApi::getShoppingListDetail(int listId,
                                           ShoppingListCallback callback)
{
    QString endpoint = QString("/api/inventory/shopping-lists/%1").arg(listId);
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            callback(false, gocook::models::ShoppingList{}, errorMsg.toStdString());
            return;
        }
        QJsonObject obj = doc.object();
        gocook::models::ShoppingList result;
        result.id = obj["id"].toInt();
        result.name = obj["name"].toString().toStdString();
        if (obj.contains("items") && obj["items"].isArray()) {
            const QJsonArray itemsArr = obj["items"].toArray();
            for (const QJsonValue& val : itemsArr) {
                QJsonObject itemObj = val.toObject();
                gocook::models::ShoppingListItem item;
                item.id = itemObj["id"].toInt();
                item.ingredient_name = itemObj["ingredient_name"].toString().toStdString();
                item.required_quantity = itemObj["required_quantity"].toDouble();
                item.inventory_quantity = itemObj["inventory_quantity"].toDouble();
                item.to_buy_quantity = itemObj["to_buy_quantity"].toDouble();
                item.unit = itemObj["unit"].toString().toStdString();
                item.checked = itemObj["checked"].toBool();
                result.items.push_back(std::move(item));
            }
        }
        callback(true, result, "");
    }, false, AuthMode::Silent);
}

void HttpGoCookApi::deleteShoppingList(int listId,
                                        SuccessCallback callback)
{
    QString endpoint = QString("/api/inventory/shopping-lists/%1").arg(listId);
    deleteResource(endpoint, {}, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (callback) callback(success, success ? "" : errorMsg.toStdString());
    }, false, AuthMode::Interactive);
}

void HttpGoCookApi::updateShoppingListItem(int listId, int itemId,
                                           const gocook::models::UpdateShoppingItemRequest& request,
                                           SuccessCallback callback)
{
    QUrl url(m_baseUrl + QString("/api/inventory/shopping-lists/%1/items/%2").arg(listId).arg(itemId));
    QNetworkRequest req(url);
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_token.isEmpty()) {
        req.setRawHeader("Authorization", QString("Bearer %1").arg(m_token).toUtf8());
    }

    QJsonObject body;
    body["checked"] = request.checked;
    QByteArray payload = QJsonDocument(body).toJson();

    sendRaw(AuthMode::Interactive, QNetworkAccessManager::CustomOperation, req, payload, "PATCH",
            [callback](int statusCode, const QByteArray &responseData) {
        QJsonDocument doc = QJsonDocument::fromJson(responseData);

        // 401 全局 unauthorized 信号已由 sendRaw 统一发出
        if (statusCode == 401) {
            if (callback) callback(false, errorMessageFor(statusCode, doc).toStdString());
            return;
        }

        if (statusCode == 200) {
            if (callback) callback(true, "");
        } else {
            if (callback) callback(false, errorMessageFor(statusCode, doc).toStdString());
        }
    });
}

void HttpGoCookApi::batchAddShoppingItems(int listId,
                                          const std::vector<gocook::models::BatchShoppingItem>& items,
                                          BatchShoppingCallback callback)
{
    QUrl url(m_baseUrl + QString("/api/inventory/shopping-lists/%1/items/batch").arg(listId));
    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_token.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_token).toUtf8());
    }

    QJsonArray arr;
    for (const auto& item : items) {
        QJsonObject obj;
        obj["ingredient_name"] = QString::fromStdString(item.ingredient_name);
        obj["quantity"] = item.quantity;
        if (!item.unit.empty())
            obj["unit"] = QString::fromStdString(item.unit);
        arr.append(obj);
    }
    QByteArray body = QJsonDocument(arr).toJson();

    sendRaw(AuthMode::Interactive, QNetworkAccessManager::PostOperation, request, body, "",
            [callback](int statusCode, const QByteArray &responseData) {
        QJsonDocument doc = QJsonDocument::fromJson(responseData);

        // 401 全局 unauthorized 信号已由 sendRaw 统一发出
        if (statusCode == 401) {
            if (callback) callback(false, gocook::models::BatchShoppingResponse{}, errorMessageFor(statusCode, doc).toStdString());
            return;
        }

        if (statusCode == 201) {
            QJsonObject obj = doc.object();
            gocook::models::BatchShoppingResponse resp;
            resp.message = obj["message"].toString().toStdString();
            if (obj.contains("items") && obj["items"].isArray()) {
                const QJsonArray itemsArr = obj["items"].toArray();
                for (const auto& val : itemsArr) {
                    QJsonObject itemObj = val.toObject();
                    gocook::models::ShoppingListItem item;
                    item.id = itemObj["id"].toInt();
                    item.ingredient_name = itemObj["ingredient_name"].toString().toStdString();
                    item.required_quantity = itemObj["required_quantity"].toDouble();
                    item.inventory_quantity = itemObj["inventory_quantity"].toDouble();
                    item.to_buy_quantity = itemObj["to_buy_quantity"].toDouble();
                    item.unit = itemObj["unit"].toString().toStdString();
                    item.checked = itemObj["checked"].toBool();
                    resp.items.push_back(std::move(item));
                }
            }
            if (callback) callback(true, resp, "");
        } else {
            // 统一三级文案：断网(statusCode=0)时不再把空 body 当错误信息抛给上层
            if (callback) callback(false, gocook::models::BatchShoppingResponse{}, errorMessageFor(statusCode, doc).toStdString());
        }
    });
}

void HttpGoCookApi::exportShoppingList(int listId,
                                        const std::string& format,
                                        std::function<void(bool, const std::string&, const std::string&)> callback)
{
    QString endpoint = QString("/api/inventory/shopping-lists/%1/export?format=%2")
        .arg(listId).arg(QString::fromStdString(format));
    QUrl url(m_baseUrl + endpoint);
    QNetworkRequest req(url);
    req.setTransferTimeout(15000);
    if (!m_token.isEmpty()) {
        req.setRawHeader("Authorization", QString("Bearer %1").arg(m_token).toUtf8());
    }

    sendRaw(AuthMode::Interactive, QNetworkAccessManager::GetOperation, req, QByteArray(), "",
            [callback](int statusCode, const QByteArray &responseData) {
        QJsonDocument doc = QJsonDocument::fromJson(responseData);

        // 401 全局 unauthorized 信号已由 sendRaw 统一发出
        if (statusCode == 401) {
            if (callback) callback(false, "", errorMessageFor(statusCode, doc).toStdString());
            return;
        }

        if (callback) {
            if (statusCode == 200) {
                callback(true, QString::fromUtf8(responseData).toStdString(), "");
            } else {
                // 统一三级文案：断网(statusCode=0)时不再把空 body 当错误信息抛给上层
                callback(false, "", errorMessageFor(statusCode, doc).toStdString());
            }
        }
    });
}

// ======================= 膳食计划 =======================
void HttpGoCookApi::createMealPlan(const gocook::models::MealPlanRequest& planData,
                                   IntCallback callback) {
    Q_UNUSED(planData);
    if (callback) callback(false, 0, "功能暂未实现");
}

void HttpGoCookApi::getMealPlans(const std::string& startDate,
                                 const std::string& endDate,
                                 int page, int size,
                                 MealPlansCallback callback) {
    Q_UNUSED(startDate); Q_UNUSED(endDate); Q_UNUSED(page); Q_UNUSED(size);
    if (callback) callback(false, gocook::models::MealPlansResponse{}, "功能暂未实现");
}

void HttpGoCookApi::getMealPlanDetail(const std::string& startDate,
                                      const std::string& endDate,
                                      int page, int size,
                                      MealPlanCalendarCallback callback) {
    Q_UNUSED(startDate); Q_UNUSED(endDate); Q_UNUSED(page); Q_UNUSED(size);
    if (callback) callback(false, gocook::models::PagedCalendarDays{}, "功能暂未实现");
}

void HttpGoCookApi::updateMealPlan(int planId,
                                   const gocook::models::MealPlanRequest& updates,
                                   SuccessCallback callback) {
    Q_UNUSED(planId); Q_UNUSED(updates);
    if (callback) callback(false, "功能暂未实现");
}

void HttpGoCookApi::deleteMealPlan(int planId,
                                   SuccessCallback callback) {
    Q_UNUSED(planId);
    if (callback) callback(false, "功能暂未实现");
}

void HttpGoCookApi::getNutritionTrend(const std::string& startDate,
                                      const std::string& endDate,
                                      NutritionTrendCallback callback) {
    Q_UNUSED(startDate); Q_UNUSED(endDate);
    if (callback) callback(false, gocook::models::NutritionTrendResponse{}, "功能暂未实现");
}

// ======================= 公告 =======================
void HttpGoCookApi::getAnnouncements(int page, int size,
                                     PagedAnnouncementsCallback callback) {
    QString endpoint = QString("/api/announcements?page=%1&size=%2").arg(page).arg(size);
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::PagedAnnouncements{}, errorMsg.toStdString());
            return;
        }
        gocook::models::PagedAnnouncements result;
        QJsonObject root = doc.object();
        if (root.contains("pagination") && root["pagination"].isObject()) {
            QJsonObject pag = root["pagination"].toObject();
            result.pagination.page        = pag["page"].toInt();
            result.pagination.size        = pag["size"].toInt();
            result.pagination.total       = pag["total"].toInt();
            result.pagination.total_pages = pag["total_pages"].toInt();
        }
        if (root.contains("data") && root["data"].isArray()) {
            const QJsonArray dataArr = root["data"].toArray();
            for (const QJsonValue& val : dataArr) {
                QJsonObject obj = val.toObject();
                gocook::models::AnnouncementItem item;
                item.id = obj["id"].toInt();
                item.title = obj["title"].toString().toStdString();
                item.content = obj["content"].toString().toStdString();
                item.created_at = obj["created_at"].toString().toStdString();
                result.data.push_back(std::move(item));
            }
        }
        if (callback) callback(true, result, "");
    }, true);
}

// ======================= 管理员功能 =======================
void HttpGoCookApi::getUsers(int page, int size,
                             const nlohmann::json& filters,
                             PagedUsersCallback callback) {
    Q_UNUSED(page); Q_UNUSED(size); Q_UNUSED(filters);
    if (callback) callback(false, gocook::models::PagedUsers{}, "功能暂未实现");
}

void HttpGoCookApi::createUser(const gocook::models::CreateUserRequest& userData,
                               SuccessCallback callback) {
    Q_UNUSED(userData);
    if (callback) callback(false, "功能暂未实现");
}

void HttpGoCookApi::updateUser(int userId,
                               const gocook::models::UpdateUserRequest& updates,
                               SuccessCallback callback) {
    Q_UNUSED(userId); Q_UNUSED(updates);
    if (callback) callback(false, "功能暂未实现");
}

void HttpGoCookApi::setUserStatus(int userId,
                                  const gocook::models::SetUserStatusRequest& request,
                                  SuccessCallback callback) {
    Q_UNUSED(userId); Q_UNUSED(request);
    if (callback) callback(false, "功能暂未实现");
}

void HttpGoCookApi::deleteUser(int userId,
                               SuccessCallback callback) {
    Q_UNUSED(userId);
    if (callback) callback(false, "功能暂未实现");
}

void HttpGoCookApi::getPendingRecipes(int page, int size,
                                      PagedPendingRecipesCallback callback) {
    Q_UNUSED(page); Q_UNUSED(size);
    if (callback) callback(false, gocook::models::PagedPendingRecipes{}, "功能暂未实现");
}

void HttpGoCookApi::approveRecipe(int recipeId,
                                  SuccessCallback callback) {
    Q_UNUSED(recipeId);
    if (callback) callback(false, "功能暂未实现");
}

void HttpGoCookApi::rejectRecipe(int recipeId,
                                 const gocook::models::RejectRecipeRequest& request,
                                 SuccessCallback callback) {
    Q_UNUSED(recipeId); Q_UNUSED(request);
    if (callback) callback(false, "功能暂未实现");
}

void HttpGoCookApi::batchReviewRecipes(const gocook::models::BatchReviewRequest& request,
                                       BatchReviewCallback callback) {
    Q_UNUSED(request);
    if (callback) callback(false, gocook::models::BatchReviewResponse{}, "功能暂未实现");
}

void HttpGoCookApi::publishAnnouncement(const gocook::models::AnnouncementRequest& request,
                                        SuccessCallback callback) {
    Q_UNUSED(request);
    if (callback) callback(false, "功能暂未实现");
}

void HttpGoCookApi::sendNotification(const gocook::models::NotificationRequest& notification,
                                     NotificationCallback callback) {
    Q_UNUSED(notification);
    if (callback) callback(false, gocook::models::NotificationResponse{}, "功能暂未实现");
}

void HttpGoCookApi::getStatistics(StatisticsCallback callback)
{
    if (callback) callback(false, gocook::models::StatisticsData{}, "功能暂未实现");
}

void HttpGoCookApi::resetTestNotifications(SuccessCallback callback)
{
    get("/api/test/reset-notifications", [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            QString err = errorStr;
            if (doc.isObject() && doc.object().contains("error"))
                err = doc.object()["error"].toString();
            if (callback) callback(false, err.toStdString());
            return;
        }
        if (callback) callback(true, "");
    });
}

void HttpGoCookApi::getAdminLogs(int page, int size,
                                  const std::string& type,
                                  int userId,
                                  PagedAdminLogsCallback callback)
{
    Q_UNUSED(page);
    Q_UNUSED(size);
    Q_UNUSED(type);
    Q_UNUSED(userId);
    if (callback) callback(false, gocook::models::PagedAdminLogs{}, "功能暂未实现");
}

void HttpGoCookApi::getActivityLogs(int page, int size,
                                     int userId,
                                     const std::string& action,
                                     PagedActivityLogsCallback callback)
{
    Q_UNUSED(page);
    Q_UNUSED(size);
    Q_UNUSED(userId);
    Q_UNUSED(action);
    if (callback) callback(false, gocook::models::PagedActivityLogs{}, "功能暂未实现");
}

// ======================= 令牌管理 =======================
void HttpGoCookApi::setAuthToken(const std::string& token) {
    setToken(QString::fromStdString(token));
}

std::string HttpGoCookApi::authToken() const {
    return token().toStdString();
}