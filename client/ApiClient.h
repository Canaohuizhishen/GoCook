#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <functional>
#include <QJSValue>

/**
 * @brief 网络请求客户端类，封装 QNetworkAccessManager 提供统一的 REST API 调用接口
 *
 * 支持 GET、POST、PUT、DELETE 四种 HTTP 方法，自动携带 Bearer Token 认证头。
 * 回调函数使用 QJSValue，可在 QML 中直接传入 JavaScript 函数。
 * 同时提供 std::function 回调重载，便于 C++ 内部调用。
 * 当收到 401 未授权响应时，会发出 unauthorized 信号，便于上层跳转登录。
 */
class ApiClient : public QObject
{
    Q_OBJECT
    // 暴露给 QML 的属性：基础 URL
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    // 暴露给 QML 的属性：认证令牌
    Q_PROPERTY(QString token READ token WRITE setToken NOTIFY tokenChanged)

public:
    // 构造函数
    explicit ApiClient(QObject *parent = nullptr);

    // 获取当前基础 URL
    QString baseUrl() const { return m_baseUrl; }
    // 设置基础 URL
    void setBaseUrl(const QString &url);

    // 获取当前令牌
    QString token() const { return m_token; }
    // 设置认证令牌
    void setToken(const QString &token);

    // 供 QML 调用的 GET 请求方法
    Q_INVOKABLE void get(const QString &endpoint, const QJSValue &callback);
    // 供 QML 调用的 POST 请求方法
    Q_INVOKABLE void post(const QString &endpoint, const QVariantMap &data, const QJSValue &callback);
    // 供 C++ 调用的 POST 请求方法（使用 std::function 回调）
    void post(const QString &endpoint, const QVariantMap &data,
              std::function<void(bool, const QString&, const QJsonDocument&)> callback);
    // 供 QML 调用的 DELETE 请求方法
    Q_INVOKABLE void deleteResource(const QString &endpoint, const QVariantMap &data, const QJSValue &callback);
    // 供 QML 调用的 PUT 请求方法
    Q_INVOKABLE void put(const QString &endpoint, const QVariantMap &data, const QJSValue &callback);

signals:
    // 基础 URL 变更信号
    void baseUrlChanged();
    // 令牌变更信号
    void tokenChanged();
    // 网络错误信号，携带错误描述
    void networkError(const QString &errorString);
    // 未授权信号（401），用于触发跳转登录
    void unauthorized();

private:
    // 内部通用请求发送方法（返回 QNetworkReply* 用于统一处理）
    QNetworkReply* sendRequestInternal(QNetworkAccessManager::Operation op,
                                       const QString &endpoint,
                                       const QVariantMap &data);
    // 统一发送 HTTP 请求的内部方法（用于 QJSValue 回调）
    void sendRequest(QNetworkAccessManager::Operation op,
                     const QString &endpoint,
                     const QVariantMap &data,
                     const QJSValue &callback);
    // 统一发送 HTTP 请求的内部方法（用于 std::function 回调）
    void sendRequest(QNetworkAccessManager::Operation op,
                     const QString &endpoint,
                     const QVariantMap &data,
                     std::function<void(bool, const QString&, const QJsonDocument&)> callback);

    // 网络访问管理器
    QNetworkAccessManager m_nam;
    // 基础 URL
    QString m_baseUrl;
    // 认证令牌
    QString m_token;
};