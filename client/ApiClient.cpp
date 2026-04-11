#include "ApiClient.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJSValue>
#include <QJSEngine>
#include <QtQml/QQmlEngine>

// 构造函数
ApiClient::ApiClient(QObject *parent) : QObject(parent)
{
    // 初始化默认基础 URL
    m_baseUrl = "http://127.0.0.1:8080";
}

// 设置基础 URL
void ApiClient::setBaseUrl(const QString &url)
{
    if (m_baseUrl != url) {
        m_baseUrl = url;
        // 发射属性变更信号
        emit baseUrlChanged();
    }
}

// 设置认证令牌
void ApiClient::setToken(const QString &token)
{
    if (m_token != token) {
        m_token = token;
        // 发射属性变更信号
        emit tokenChanged();
    }
}

// GET 请求封装
void ApiClient::get(const QString &endpoint, const QJSValue &callback)
{
    // 调用统一请求发送方法
    sendRequest(QNetworkAccessManager::GetOperation, endpoint, QVariantMap(), callback);
}

// POST 请求封装（QML 版本）
void ApiClient::post(const QString &endpoint, const QVariantMap &data, const QJSValue &callback)
{
    // 调用统一请求发送方法
    sendRequest(QNetworkAccessManager::PostOperation, endpoint, data, callback);
}

// POST 请求封装（C++ 版本，std::function 回调）
void ApiClient::post(const QString &endpoint, const QVariantMap &data,
                     std::function<void(bool, const QString&, const QJsonDocument&)> callback)
{
    sendRequest(QNetworkAccessManager::PostOperation, endpoint, data, std::move(callback));
}

// DELETE 请求封装
void ApiClient::deleteResource(const QString &endpoint, const QVariantMap &data, const QJSValue &callback)
{
    // 调用统一请求发送方法
    sendRequest(QNetworkAccessManager::DeleteOperation, endpoint, data, callback);
}

// PUT 请求封装
void ApiClient::put(const QString &endpoint, const QVariantMap &data, const QJSValue &callback)
{
    // 调用统一请求发送方法
    sendRequest(QNetworkAccessManager::PutOperation, endpoint, data, callback);
}

// 内部通用请求发送（构造请求、发送，返回 QNetworkReply*）
QNetworkReply* ApiClient::sendRequestInternal(QNetworkAccessManager::Operation op,
                                              const QString &endpoint,
                                              const QVariantMap &data)
{
    // 构造完整 URL
    QUrl url(m_baseUrl + endpoint);
    QNetworkRequest request(url);
    // 设置内容类型头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // 如果存在令牌，则添加 Authorization 头
    if (!m_token.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_token).toUtf8());
    }

    // 将数据转换为 JSON 字节数组
    QByteArray body;
    if (!data.isEmpty()) {
        body = QJsonDocument(QJsonObject::fromVariantMap(data)).toJson();
    }

    QNetworkReply *reply = nullptr;
    // 根据操作类型发送请求
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
    default:
        break;
    }
    return reply;
}

// 统一发送 HTTP 请求的实现（QJSValue 回调版本）
void ApiClient::sendRequest(QNetworkAccessManager::Operation op,
                            const QString &endpoint,
                            const QVariantMap &data,
                            const QJSValue &callback)
{
    QNetworkReply *reply = sendRequestInternal(op, endpoint, data);

    //reply 空指针检查
    if (!reply) {
        // 如果有可调用的 QML 回调，传入失败信息
        if (callback.isCallable()) {
            QJSValueList args;
            args << false << "Failed to create network request" << QJSValue();
            QJSValue(callback).call(args);
        }
        // 发射网络错误信号
        emit networkError("Failed to create network request");
        return;
    }

    // 连接请求完成信号
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        // 请求完成后自动删除 reply 对象
        reply->deleteLater();

        // 获取 HTTP 状态码
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 如果是 401，发射未授权信号
        if (statusCode == 401) {
            emit unauthorized();
        }

        // 如果有网络错误，发射错误信号并回调失败
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            if (callback.isCallable()) {
                QJSValueList args;
                args << false << reply->errorString() << QJSValue();
                QJSValue(callback).call(args);
            }
            return;
        }

        // 读取响应数据
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        // 判断是否成功（2xx 状态码）
        bool success = (statusCode >= 200 && statusCode < 300);

        // 执行 QML 回调
        if (callback.isCallable()) {
            // 获取 ApiClient 关联的 JS 引擎
            QJSEngine *engine = qjsEngine(this);
            QJSValue jsResponse;
            if (engine) {
                // 将 JSON 转换为脚本值
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
                            // 若数组中嵌套数组，递归处理（可根据需要扩展）
                            jsResponse.setProperty(i, engine->toScriptValue(value.toArray().toVariantList()));
                        } else {
                            jsResponse.setProperty(i, engine->toScriptValue(value.toVariant()));
                        }
                    }
                } else {
                    jsResponse = engine->newObject();
                }
            } else {
                // 无引擎时的后备
                jsResponse = QJSValue();
            }

            // 构建回调参数列表
            QJSValueList args;
            args << success;
            args << (success ? QString() : QString::fromUtf8(responseData));
            args << jsResponse;
            // 调用 QML 传入的回调函数
            QJSValue(callback).call(args);
        }
    });
}

// 统一发送 HTTP 请求的实现（std::function 回调版本）
void ApiClient::sendRequest(QNetworkAccessManager::Operation op,
                            const QString &endpoint,
                            const QVariantMap &data,
                            std::function<void(bool, const QString&, const QJsonDocument&)> callback)
{
    QNetworkReply *reply = sendRequestInternal(op, endpoint, data);
    if (!reply) {
        // 若请求未能发出，直接回调失败
        if (callback) {
            callback(false, "Failed to create network request", QJsonDocument());
        }
        return;
    }

    // 连接请求完成信号
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback = std::move(callback)]() {
        // 请求完成后自动删除 reply 对象
        reply->deleteLater();

        // 获取 HTTP 状态码
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 如果是 401，发射未授权信号
        if (statusCode == 401) {
            emit unauthorized();
        }

        // 如果有网络错误，发射错误信号并回调失败
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            if (callback) {
                callback(false, reply->errorString(), QJsonDocument());
            }
            return;
        }

        // 读取响应数据
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        // 判断是否成功（2xx 状态码）
        bool success = (statusCode >= 200 && statusCode < 300);

        // 调用 C++ 回调
        if (callback) {
            callback(success, success ? QString() : QString::fromUtf8(responseData), doc);
        }
    });
}