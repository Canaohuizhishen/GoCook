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
#include "JsonDeserializer.h"

// 统一错误文案解析（HTTP 与登录守卫错误文案的唯一出口；kAuthRequiredError 即出自这里）。
// 判据（自上而下，首个命中者生效）：
//   statusCode == -2      → 登录守卫拦截（请求未发出），文案 = kAuthRequiredError
//   statusCode <= 0       → 网络层错误（断网/拒绝连接/超时），无服务端响应
//   statusCode == 503     → 服务器繁忙（连接池饱和等瞬时故障），归一为 kServerBusyErrorMessage
//                           （不透传响应体：文案与重试判定需稳定，不随服务端措辞漂移）
//   响应 JSON 含非空 error → 业务错误，用服务端精确文案
//   其余（HTTP > 0）      → 服务器问题（5xx 返回 HTML/空体等），不误导用户去查网络
static QString errorMessageFor(int statusCode, const QJsonDocument &doc)
{
    // -2 = 被登录守卫拦截（未登录且接口需登录），未发送任何请求
    if (statusCode == -2) {
        return HttpGoCookApi::kAuthRequiredError;
    }
    if (statusCode <= 0) {
        return HttpGoCookApi::kNetworkErrorMessage;
    }
    // 503 = 服务器瞬时繁忙：客户端归一为固定文案（VM 据此自动退避重试）
    if (statusCode == 503) {
        return HttpGoCookApi::kServerBusyErrorMessage;
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

HttpGoCookApi::HttpGoCookApi(QObject *parent) : QObject(parent)
{
    // 初始化默认基础 URL
    m_baseUrl = "http://127.0.0.1:8080";
}

void HttpGoCookApi::setBaseUrl(const QString &url)
{
    if (m_baseUrl != url) {
        m_baseUrl = url;
        emit baseUrlChanged();
    }
}

void HttpGoCookApi::setToken(const QString &token)
{
    if (m_token != token) {
        m_token = token;
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

    // 先搬走再遍历：handler 内部可能重入操作成员队列，直接遍历成员会迭代器失效
    auto pending = std::move(m_pendingAuthRequests);
    m_pendingAuthRequests.clear();
    for (auto &p : pending) {
        if (p.handler)
            // p.handler 是 sendRequest() 传给 sendRaw() 的“统一收尾 lambda”
            // （不是 PendingAuthRequest 现场构造的），它捕获了 ViewModel 传进来的 callback。
            // 挂起时由 sendRaw() 在“未登录 + Interactive”分支 move 进队列；
            // 此处固定以 -2 调用它 → handler 内走 -2 分支 → errorMessageFor(-2)
            // 返回 kAuthRequiredError("请先登录") → 经 callback 一路传回 ViewModel。
            p.handler(-2, QByteArray());
    }
}

// QML 可调用：取消全部挂起的 Interactive 请求（应用内登录页被关闭/跳过时）
void HttpGoCookApi::cancelAuthQueue()
{
    clearPendingAuthRequests();
}

void HttpGoCookApi::setMaxRetries(int retries)
{
    if (m_maxRetries != retries) {
        m_maxRetries = retries;
        emit maxRetriesChanged();
    }
}

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
QNetworkRequest HttpGoCookApi::buildRequest(const QString &endpoint)
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
        // 网络恢复检测（down→up 边沿，覆盖含手写路径在内的全部请求）：
        // statusCode<=0 = 网络层错误（断网/超时，未收到任何服务端响应）→ 记失联；
        // 收到任意真实响应（>0）且此前处于失联 → 视为恢复，emit 一次
        if (statusCode <= 0) {
            self->m_networkDown = true;
        } else if (self->m_networkDown) {
            self->m_networkDown = false;
            emit self->networkRestored();
        }
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
    auto wrapped = [this, callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
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
        args << errorMsg;
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
    QNetworkRequest request = buildRequest(endpoint);
    // 将数据转换为 JSON 字节数组
    QByteArray body;
    if (!data.isEmpty()) {
        body = QJsonDocument(QJsonObject::fromVariantMap(data)).toJson();
    }
    // 无请求体的 DELETE/PATCH（经 sendCustomRequest 发送的自定义方法）：Qt 不会写 Content-Length 头，
    // 而服务端 httplib 把 DELETE 一律视为“有内容”请求：无 Content-Length 时读体逻辑会阻塞等待请求体，
    // 响应永不返回（客户端挂到 15s 传输超时）。发一个最小 JSON 体“{}”保证 Content-Length 恒存在。
    if (body.isEmpty() && (op == QNetworkAccessManager::DeleteOperation ||
                           op == QNetworkAccessManager::CustomOperation)) {
        body = QByteArrayLiteral("{}");
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

        // -1 = 请求未能发出（sendRaw 极端分支：reply 构造失败）：不重试，直接按失败回调
        // （文案沿用网络层错误；与其它失败一样受 suppressNetworkError 抑制）
        if (statusCode == -1) {
            const QString errMsg = errorMessageFor(statusCode, doc);
            if (!suppressNetworkError) emit self->networkError(errMsg);
            if (callback) callback(false, errMsg, doc);
            return;
        }

        // 网络层错误（无任何 HTTP 响应，statusCode<=0；-1/-2 已提前分流）：只对 GET 重试；
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

// ==================== IGoCookApi 抽象接口实现 ======================
// 本区书写约定（新增/修改接口方法时按此执行，例外必须就近注释）：
//   1. AuthMode 声明：需登录的读接口（列表/详情/加载）→ Silent；写操作/登录态敏感 → Interactive；
//      对游客开放 → Public（菜谱浏览/公告等公开接口优先按此档）。参考 getCurrentUser 的例外注释写法。
//   2. 错误文案：统一用 errorMessageFor（已含服务端 error 文案）；方法内不要手工二次提取响应 error 字段。
//   3. suppressNetworkError：错误由 VM/页面呈现的接口传 true；故意让全局 toast 兜底的传
//      false 并就近注释原因（当前 false：我的投稿、购物清单四方法）。
//   4. 成功判定：统一 2xx 区间（statusCode >= 200 && statusCode < 300）。
//   5. 未实现接口：用区块级 TODO 标记（见膳食计划/管理员功能区），不留裸存根。

// ======================= 认证 =======================
void HttpGoCookApi::registerUser(const gocook::models::RegisterRequest& request,
                                 SuccessCallback callback)
{
    QVariantMap data;
    data["username"] = QString::fromStdString(request.username);
    data["password"] = QString::fromStdString(request.password);
    data["email"]    = QString::fromStdString(request.email);

    post("/api/register", data, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (success) {
            if (callback) callback(true, "");
        } else {
            if (callback) callback(false, errorMsg.toStdString());
        }
    }, true);
}

void HttpGoCookApi::verifyRegistration(const std::string& email,
                                       const std::string& token,
                                       SuccessCallback callback)
{
    QVariantMap data;
    data["email"] = QString::fromStdString(email);
    data["token"] = QString::fromStdString(token);

    post("/api/register/verify", data, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (success) {
            if (callback) callback(true, "");
        } else {
            if (callback) callback(false, errorMsg.toStdString());
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
            if (callback) callback(false, gocook::models::LoginResponse{}, errorMsg.toStdString());
            return;
        }
        if (doc.isObject()) {
            gocook::models::LoginResponse resp =
                JsonDeserializer::parseLoginResponse(doc.object());
            if (callback) callback(true, resp, "");
        } else {
            if (callback) callback(false, gocook::models::LoginResponse{}, "无效的响应格式");
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

    post("/api/password/forgot", data, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (success) {
            if (callback) callback(true, "");
        } else {
            if (callback) callback(false, errorMsg.toStdString());
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

    post("/api/password/reset", data, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (success) {
            if (callback) callback(true, "");
        } else {
            if (callback) callback(false, errorMsg.toStdString());
        }
    }, true);
}

// ======================= 用户相关 =======================
void HttpGoCookApi::getCurrentUser(UserProfileCallback callback) {
    // Silent：登录态校验/加载类，未登录静默失败（checkAutoLogin 仅在本地有 token 时调用，不受影响）
    get("/api/users/me", [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::UserProfile{}, errorStr.toStdString());
            return;
        }
        gocook::models::UserProfile profile =
            JsonDeserializer::parseUserProfile(doc.object());
        if (callback) callback(true, profile, "");
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

    put("/api/users/me/profile", data, [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::UserProfile{}, errorStr.toStdString());
            return;
        }
        gocook::models::UserProfile profile =
            JsonDeserializer::parseUserProfile(doc.object());
        if (callback) callback(true, profile, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::getPreferences(PreferencesCallback callback) {
    get("/api/users/me/preferences", [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::UserPreferences{}, errorStr.toStdString());
            return;
        }
        gocook::models::UserPreferences prefs =
            JsonDeserializer::parseUserPreferences(doc.object());
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

    put("/api/users/me/preferences", data, [callback](bool success, const QString& errorStr, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorStr.toStdString());
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
            if (callback) callback(false, gocook::models::HealthProfileResponse{}, errorStr.toStdString());
            return;
        }

        gocook::models::HealthProfileResponse resp =
            JsonDeserializer::parseHealthProfileResponse(doc.object());
        if (callback) callback(true, resp, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::getHealthProfile(HealthProfileCallback callback) {
    get("/api/users/me/health-profile", [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::HealthProfileResponse{}, errorStr.toStdString());
            return;
        }

        gocook::models::HealthProfileResponse resp =
            JsonDeserializer::parseHealthProfileResponse(doc.object());
        if (callback) callback(true, resp, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::uploadAvatar(const std::string& filePath,
                                 AvatarUploadCallback callback)
{
    QFile file(QString::fromStdString(filePath));
    if (!file.exists()) {
        if (callback) callback(false, gocook::models::AvatarUploadResponse{}, "文件不存在");
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (callback) callback(false, gocook::models::AvatarUploadResponse{}, "无法打开文件");
        return;
    }
    QByteArray fileData = file.readAll();
    QString fileName = QFileInfo(file.fileName()).fileName();
    file.close();
    if (fileData.size() > 5 * 1024 * 1024) {
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
    QUrl url(m_baseUrl + "/api/users/me/avatar");
    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (!m_token.isEmpty())
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_token).toUtf8());
    sendRaw(AuthMode::Interactive, QNetworkAccessManager::PostOperation, request, fileData, "",
            [callback](int statusCode, const QByteArray &responseData) {
        QJsonDocument doc = QJsonDocument::fromJson(responseData);

        if (statusCode == 401) {
            if (callback) callback(false, gocook::models::AvatarUploadResponse{}, errorMessageFor(statusCode, doc).toStdString());
            return;
        }

        bool success = (statusCode >= 200 && statusCode < 300);
        if (!success) {
            const QString err = errorMessageFor(statusCode, doc);
            if (callback) callback(false, gocook::models::AvatarUploadResponse{}, err.toStdString());
            return;
        }

        if (doc.isObject()) {
            gocook::models::AvatarUploadResponse resp =
                JsonDeserializer::parseAvatarUploadResponse(doc.object());
            if (resp.avatar_url.empty()) {
                // 暂存引用是"保存绑定"的唯一凭据：缺失即视为无效响应（防静默进入无引用 pending）
                if (callback) callback(false, gocook::models::AvatarUploadResponse{}, "无效的响应格式");
                return;
            }
            if (callback) callback(true, resp, "");
        } else {
            if (callback) callback(false, gocook::models::AvatarUploadResponse{}, "无效的响应格式");
        }
    });
}

void HttpGoCookApi::discardPendingAvatar(const std::string& avatarUrl,
                                         SuccessCallback callback)
{
    // 放弃未保存的暂存头像（编辑页"返回"时调用）：DELETE 带 JSON 体，
    // 经 deleteResource → sendRequest 收口（Content-Length 恒存在，规避 httplib 无体 DELETE 挂死）
    QVariantMap data;
    data["avatar_url"] = QString::fromStdString(avatarUrl);
    deleteResource(QStringLiteral("/api/users/me/avatar"), data,
        [callback](bool success, const QString& errorStr, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorStr.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
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

    QUrl url(m_baseUrl + QString("/api/recipes/%1/steps/%2/image").arg(recipeId).arg(stepIndex));
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
            const QString err = errorMessageFor(statusCode, doc);
            if (callback) callback(false, "", err.toStdString());
            return;
        }

        if (doc.isObject()) {
            std::string imageUrl;
            if (doc.object().contains("image_url"))
                imageUrl = doc.object()["image_url"].toString().toStdString();
            if (callback) callback(true, imageUrl, "");
        } else {
            if (callback) callback(false, "", "无效的响应格式");
        }
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
        }, true, AuthMode::Interactive);
}

void HttpGoCookApi::changePassword(const std::string& currentPassword,
                                   const std::string& newPassword,
                                   SuccessCallback callback)
{
    QVariantMap data;
    data["current_password"] = QString::fromStdString(currentPassword);
    data["new_password"] = QString::fromStdString(newPassword);

    put("/api/users/me/password", data, [callback](bool success, const QString& errorStr, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorStr.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::deleteAccount(SuccessCallback callback)
{
    deleteResource("/api/users/me", {}, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorMsg.toStdString());
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
        gocook::models::PagedFavorites result =
            JsonDeserializer::parsePagedFavorites(doc.object());
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
        for (const QJsonValue& v : doc.array())
            groups.push_back(JsonDeserializer::parseFavoriteGroup(v.toObject()));
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
        gocook::models::FavoriteGroup group =
            JsonDeserializer::parseFavoriteGroup(doc.object());
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
            if (callback) callback(false, errorStr.toStdString());
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

void HttpGoCookApi::batchUpdateFavorites(const gocook::models::BatchUpdateFavoritesRequest& request,
                                         SuccessCallback callback) {
    QVariantMap data;
    QVariantList ids;
    for (const auto& fid : request.favorite_ids)
        ids.append(fid);
    data["favorite_ids"] = ids;
    if (request.group_id.has_value())
        data["group_id"] = request.group_id.value();
    if (request.is_public.has_value())
        data["is_public"] = request.is_public.value();

    QUrl url(m_baseUrl + QStringLiteral("/api/users/me/favorites/batch"));
    QNetworkRequest req(url);
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", QString("Bearer %1").arg(m_token).toUtf8());

    QByteArray body = QJsonDocument(QJsonObject::fromVariantMap(data)).toJson();
    sendRaw(AuthMode::Interactive, QNetworkAccessManager::CustomOperation, req, body, "PATCH",
            [callback](int statusCode, const QByteArray &responseData) {
        QJsonDocument doc = QJsonDocument::fromJson(responseData);

        if (statusCode == 401) {
            if (callback) callback(false, errorMessageFor(statusCode, doc).toStdString());
            return;
        }

        bool success = (statusCode >= 200 && statusCode < 300);
        if (callback) {
            if (success)
                callback(true, "");
            else
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
        gocook::models::PagedNotifications result =
            JsonDeserializer::parsePagedNotifications(doc.object());
        if (callback) callback(true, result, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::getNotificationsUnreadSummary(UnreadSummaryCallback callback)
{
    // 角标汇总：后台加载类——未登录不发送（Silent）+ 失败不上全局 toast（由 VM 保留旧缓存）
    get("/api/users/me/notifications/unread-summary",
        [callback](bool success, const QString& errorStr, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::NotificationUnreadSummary{}, errorStr.toStdString());
            return;
        }
        gocook::models::NotificationUnreadSummary summary =
            JsonDeserializer::parseNotificationUnreadSummary(doc.object());
        if (callback) callback(true, summary, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::setNotificationsReadState(const std::string& type, int lastSeenId,
                                              SuccessCallback callback)
{
    // 进页即已读的水位上报：后台行为，失败静默（下一轮汇总会自愈）
    QVariantMap body;
    body["type"] = QString::fromStdString(type);
    body["last_seen_id"] = lastSeenId;
    put("/api/users/me/notifications/read-state", body,
        [callback](bool success, const QString& errorStr, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorStr.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::setAnnouncementsReadState(int lastSeenId,
                                              SuccessCallback callback)
{
    // 系统通知页红点上报：后台行为，失败静默
    QVariantMap body;
    body["last_seen_id"] = lastSeenId;
    put("/api/users/me/announcements/read-state", body,
        [callback](bool success, const QString& errorStr, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorStr.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::deleteNotification(int notificationId,
                                        SuccessCallback callback)
{
    deleteResource(QString("/api/users/me/notifications/%1").arg(notificationId), {},
        [callback](bool success, const QString& errorStr, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorStr.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

// ======================= 菜谱相关 =======================

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
            if (callback) callback(false, gocook::models::PagedRecipes{},
                                   errorMsg.toStdString());
            return;
        }
        gocook::models::PagedRecipes result =
            JsonDeserializer::parsePagedRecipes(doc.object());
        if (callback) callback(true, result, "");
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
            if (callback) callback(false, gocook::models::PagedRecommendedRecipes{},
                                   errorMsg.toStdString());
            return;
        }
        gocook::models::PagedRecommendedRecipes result =
            JsonDeserializer::parsePagedRecommendedRecipes(doc.object());
        if (callback) callback(true, result, "");
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
            if (callback) callback(false, gocook::models::PagedRecipes{},
                                   errorMsg.toStdString());
            return;
        }
        gocook::models::PagedRecipes result =
            JsonDeserializer::parsePagedRecipes(doc.object());
        if (callback) callback(true, result, "");
    }, true);
}

void HttpGoCookApi::getRecipeDetail(int recipeId,
                                    RecipeDetailCallback callback) {
    QString endpoint = QString("/api/recipes/%1").arg(recipeId);
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::RecipeDetail{}, errorMsg.toStdString());
            return;
        }
        gocook::models::RecipeDetail detail =
            JsonDeserializer::parseRecipeDetail(doc.object());
        if (callback) callback(true, detail, "");
    }, true);
}

void HttpGoCookApi::getRecipeVideos(int recipeId,
                                    RecipeVideosCallback callback) {
    QString endpoint = QString("/api/recipes/%1/videos").arg(recipeId);
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, {}, errorMsg.toStdString());
            return;
        }
        std::vector<gocook::models::RecipeVideo> videos;
        for (const QJsonValue& val : doc.array())
            videos.push_back(JsonDeserializer::parseRecipeVideo(val.toObject()));
        if (callback) callback(true, videos, "");
    }, true);
}

void HttpGoCookApi::getRecipeRatings(int recipeId, int page, int size,
                                     PagedRatingsCallback callback) {
    QString endpoint = QString("/api/recipes/%1/ratings?page=%2&size=%3")
                           .arg(recipeId).arg(page).arg(size);
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::PagedRatings{}, errorMsg.toStdString());
            return;
        }
        gocook::models::PagedRatings result =
            JsonDeserializer::parsePagedRatings(doc.object());
        if (callback) callback(true, result, "");
    }, true);
}

void HttpGoCookApi::getMyRecipeRating(int recipeId,
                                       MyRecipeRatingCallback callback) {
    QString endpoint = QString("/api/recipes/%1/ratings/mine").arg(recipeId);
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, std::nullopt, errorMsg.toStdString());
            return;
        }
        const QJsonObject obj = doc.object();
        if (obj.isEmpty()) {
            if (callback) callback(true, std::nullopt, "");
            return;
        }
        gocook::models::RecipeRating r = JsonDeserializer::parseRecipeRating(obj);
        if (callback) callback(true, std::move(r), "");
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
            if (callback) callback(false, gocook::models::SubmitRecipeResponse{}, errorMsg.toStdString());
            return;
        }
        gocook::models::SubmitRecipeResponse resp =
            JsonDeserializer::parseSubmitRecipeResponse(doc.object());
        if (callback) callback(true, resp, "");
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

    // suppressNetworkError=false：故意让全局 toast 兜底（页面不另行呈现）
    get(endpoint, [callback](bool success, const QString &errorMsg, const QJsonDocument &doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::PagedMyRecipes{},
                                   errorMsg.toStdString());
            return;
        }
        gocook::models::PagedMyRecipes result =
            JsonDeserializer::parsePagedMyRecipes(doc.object());
        if (callback) callback(true, result, "");
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
            if (callback) callback(false, errorMsg.toStdString());
            return;
        }
        if (callback) callback(true, "");
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
    }, true, AuthMode::Interactive);
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
    }, true, AuthMode::Interactive);
}

void HttpGoCookApi::deleteRating(int recipeId, int ratingId,
                                  SuccessCallback callback)
{
    QString endpoint = QString("/api/recipes/%1/ratings/%2").arg(recipeId).arg(ratingId);
    deleteResource(endpoint, QVariantMap{}, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (callback) callback(success, errorMsg.toStdString());
    }, true, AuthMode::Interactive);
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
                                   errorMsg.toStdString());
            return;
        }
        gocook::models::PagedUserRatings result =
            JsonDeserializer::parsePagedUserRatings(doc.object());
        if (callback) callback(true, result, "");
    }, true, AuthMode::Silent);
}

void HttpGoCookApi::getRecipeNutrition(int recipeId,
                                        NutritionReportCallback callback)
{
    QString endpoint = QString("/api/recipes/%1/nutrition").arg(recipeId);
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::NutritionReport{}, errorMsg.toStdString());
            return;
        }
        gocook::models::NutritionReport report =
            JsonDeserializer::parseNutritionReport(doc.object());

        if (callback) callback(true, report, "");
    }, true);
}

// ======================= 库存管理 =======================
void HttpGoCookApi::getInventory(int page, int size, const std::string& keyword,
                                 PagedInventoryCallback callback) {
    QUrlQuery query;
    query.addQueryItem("page", QString::number(page));
    query.addQueryItem("size", QString::number(size));
    // 库存页过滤框（v2.14）：keyword 非空才拼入查询串
    if (!keyword.empty())
        query.addQueryItem("keyword", QString::fromStdString(keyword));
    QString endpoint = "/api/inventory?" + query.toString(QUrl::FullyEncoded);

    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::PagedInventory{}, errorMsg.toStdString());
            return;
        }
        gocook::models::PagedInventory result =
            JsonDeserializer::parsePagedInventory(doc.object());
        if (callback) callback(true, result, "");
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
            if (callback) callback(false, 0, errorMsg.toStdString());
            return;
        }
        int id = doc.object()["id"].toInt();
        if (callback) callback(true, id, "");
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
            if (!success) {
                if (callback) callback(false, errorMsg.toStdString());
                return;
            }
            if (callback) callback(true, "");
        }, true, AuthMode::Interactive);
}

void HttpGoCookApi::deleteInventoryItem(int itemId,
                                        SuccessCallback callback) {
    QString endpoint = QString("/api/inventory/%1").arg(itemId);
    deleteResource(endpoint, {}, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorMsg.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, true, AuthMode::Interactive);
}

// ======================= 购物清单 / 膳食计划 =======================
void HttpGoCookApi::getShoppingLists(ShoppingListsCallback callback)
{
    // suppressNetworkError=false：故意让全局 toast 兜底（页面不另行呈现）
    get("/api/inventory/shopping-lists", [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, std::vector<gocook::models::ShoppingListSummary>{}, errorMsg.toStdString());
            return;
        }
        std::vector<gocook::models::ShoppingListSummary> result;
        for (const QJsonValue& val : doc.array())
            result.push_back(JsonDeserializer::parseShoppingListSummary(val.toObject()));
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
    // v2.18 可选 items：建单同时携带条目（服务端单事务落库；缺省时行为与旧版一致）
    if (!request.items.empty()) {
        QVariantList items;
        for (const auto& item : request.items) {
            QVariantMap obj;
            obj["ingredient_name"] = QString::fromStdString(item.ingredient_name);
            obj["quantity"] = item.quantity;
            if (!item.unit.empty())
                obj["unit"] = QString::fromStdString(item.unit);
            items.append(obj);
        }
        data["items"] = items;
    }

    // suppressNetworkError=false：故意让全局 toast 兜底（页面不另行呈现）
    post("/api/inventory/shopping-lists", data, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::ShoppingList{}, errorMsg.toStdString());
            return;
        }
        gocook::models::ShoppingList list =
            JsonDeserializer::parseShoppingList(doc.object());
        if (callback) callback(true, list, "");
    }, false, AuthMode::Interactive);
}

void HttpGoCookApi::getShoppingListDetail(int listId,
                                           ShoppingListCallback callback)
{
    QString endpoint = QString("/api/inventory/shopping-lists/%1").arg(listId);
    // suppressNetworkError=false：故意让全局 toast 兜底（页面不另行呈现）
    get(endpoint, [callback](bool success, const QString& errorMsg, const QJsonDocument& doc) {
        if (!success) {
            if (callback) callback(false, gocook::models::ShoppingList{}, errorMsg.toStdString());
            return;
        }
        gocook::models::ShoppingList result =
            JsonDeserializer::parseShoppingList(doc.object());
        if (callback) callback(true, result, "");
    }, false, AuthMode::Silent);
}

void HttpGoCookApi::deleteShoppingList(int listId,
                                        SuccessCallback callback)
{
    QString endpoint = QString("/api/inventory/shopping-lists/%1").arg(listId);
    // suppressNetworkError=false：故意让全局 toast 兜底（页面不另行呈现）
    deleteResource(endpoint, {}, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorMsg.toStdString());
            return;
        }
        if (callback) callback(true, "");
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

        if (statusCode == 401) {
            if (callback) callback(false, errorMessageFor(statusCode, doc).toStdString());
            return;
        }

        if (statusCode >= 200 && statusCode < 300) {
            if (callback) callback(true, "");
        } else {
            if (callback) callback(false, errorMessageFor(statusCode, doc).toStdString());
        }
    });
}

void HttpGoCookApi::deleteShoppingListItem(int listId, int itemId,
                                           SuccessCallback callback)
{
    QString endpoint = QString("/api/inventory/shopping-lists/%1/items/%2").arg(listId).arg(itemId);
    // 经 deleteResource → sendRequest 收口（Content-Length 恒存在，规避 httplib 无体 DELETE 挂死）
    // suppressNetworkError=false：故意让全局 toast 兜底（页面不另行呈现）
    deleteResource(endpoint, {}, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorMsg.toStdString());
            return;
        }
        if (callback) callback(true, "");
    }, false, AuthMode::Interactive);
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

        if (statusCode == 401) {
            if (callback) callback(false, gocook::models::BatchShoppingResponse{}, errorMessageFor(statusCode, doc).toStdString());
            return;
        }

        if (statusCode >= 200 && statusCode < 300) {
            gocook::models::BatchShoppingResponse resp =
                JsonDeserializer::parseBatchShoppingResponse(doc.object());
            if (callback) callback(true, resp, "");
        } else {
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

        if (statusCode == 401) {
            if (callback) callback(false, "", errorMessageFor(statusCode, doc).toStdString());
            return;
        }

        if (callback) {
            if (statusCode >= 200 && statusCode < 300) {
                callback(true, QString::fromUtf8(responseData).toStdString(), "");
            } else {
                callback(false, "", errorMessageFor(statusCode, doc).toStdString());
            }
        }
    });
}

// ======================= 膳食计划 =======================
// TODO(未实现)：服务端接口未就绪，本区方法固定按失败回调占位（error="功能暂未实现"）。
// 启用时需同步：本区实现 + 客户端调用方；确认弃用则删除声明。
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
        gocook::models::PagedAnnouncements result =
            JsonDeserializer::parsePagedAnnouncements(doc.object());
        if (callback) callback(true, result, "");
    }, true);
}

// ======================= 管理员功能 =======================
// TODO(未实现)：本区方法除 resetTestNotifications 外均为占位（固定失败回调 error="功能暂未实现"）。
// 启用时需同步：本区实现 + 客户端调用方；确认弃用则删除声明。
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

// 测试辅助（仅开发环境可用，见 IGoCookApi.h）。
void HttpGoCookApi::resetTestNotifications(SuccessCallback callback)
{
    get("/api/test/reset-notifications", [callback](bool success, const QString& errorStr, const QJsonDocument&) {
        if (!success) {
            if (callback) callback(false, errorStr.toStdString());
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