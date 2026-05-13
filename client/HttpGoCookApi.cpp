#include "HttpGoCookApi.h"
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJSValue>
#include <QJSEngine>
#include <QtQml/QQmlEngine>
#include <QTimer>
#include <gocook/IServices.h>

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
    }
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
void HttpGoCookApi::get(const QString &endpoint, const QJSValue &callback)
{
    // 调用统一请求发送方法，retryCount 从 0 开始
    sendRequest(QNetworkAccessManager::GetOperation, endpoint, QVariantMap(), callback, 0);
}

// GET 请求封装（C++ 版本，std::function 回调）
void HttpGoCookApi::get(const QString &endpoint,
                        std::function<void(bool, const QString&, const QJsonDocument&)> callback)
{
    sendRequest(QNetworkAccessManager::GetOperation, endpoint, QVariantMap(), std::move(callback), 0);
}

// POST 请求封装（QML 版本）
void HttpGoCookApi::post(const QString &endpoint, const QVariantMap &data, const QJSValue &callback)
{
    sendRequest(QNetworkAccessManager::PostOperation, endpoint, data, callback, 0);
}

// POST 请求封装（C++ 版本，std::function 回调）
void HttpGoCookApi::post(const QString &endpoint, const QVariantMap &data,
                         std::function<void(bool, const QString&, const QJsonDocument&)> callback)
{
    sendRequest(QNetworkAccessManager::PostOperation, endpoint, data, std::move(callback), 0);
}

// DELETE 请求封装（QML 版本）
void HttpGoCookApi::deleteResource(const QString &endpoint, const QVariantMap &data, const QJSValue &callback)
{
    sendRequest(QNetworkAccessManager::DeleteOperation, endpoint, data, callback, 0);
}

// DELETE 请求封装（C++ 版本，std::function 回调）
void HttpGoCookApi::deleteResource(const QString &endpoint, const QVariantMap &data,
                                   std::function<void(bool, const QString&, const QJsonDocument&)> callback)
{
    sendRequest(QNetworkAccessManager::DeleteOperation, endpoint, data, std::move(callback), 0);
}

// PUT 请求封装（QML 版本）
void HttpGoCookApi::put(const QString &endpoint, const QVariantMap &data, const QJSValue &callback)
{
    sendRequest(QNetworkAccessManager::PutOperation, endpoint, data, callback, 0);
}

// PUT 请求封装（C++ 版本，std::function 回调）
void HttpGoCookApi::put(const QString &endpoint, const QVariantMap &data,
                        std::function<void(bool, const QString&, const QJsonDocument&)> callback)
{
    sendRequest(QNetworkAccessManager::PutOperation, endpoint, data, std::move(callback), 0);
}

// 内部通用请求发送（构造请求、发送，返回 QNetworkReply*）
QNetworkReply* HttpGoCookApi::sendRequestInternal(QNetworkAccessManager::Operation op,
                                                  const QString &endpoint,
                                                  const QVariantMap &data)
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

// 统一发送 HTTP 请求的实现（QJSValue 回调版本），retryCount 用于重试控制
void HttpGoCookApi::sendRequest(QNetworkAccessManager::Operation op,
                                const QString &endpoint,
                                const QVariantMap &data,
                                const QJSValue &callback,
                                int retryCount)
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
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, op, endpoint, data, retryCount]() {
        // 请求完成后自动删除 reply 对象
        reply->deleteLater();

        // 获取 HTTP 状态码
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 如果是 401，发射未授权信号，并触发抽象层回调
        if (statusCode == 401) {
            emit unauthorized();
            invokeUnauthorizedHandler();
        }

        // 如果有网络错误，发射错误信号并回调失败
        if (reply->error() != QNetworkReply::NoError) {
            // 检查是否需要重试：仅对 GET 操作，且重试计数未达上限
            if (op == QNetworkAccessManager::GetOperation && retryCount < m_maxRetries) {
                // 延迟后重试，递增重试计数
                QTimer::singleShot(m_retryDelay, this, [this, op, endpoint, data, callback, retryCount]() {
                    sendRequest(op, endpoint, data, callback, retryCount + 1);
                });
                return;
            }

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
            // 获取 HttpGoCookApi 关联的 JS 引擎
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
                // 无引擎时的后备：传递 null，QML 侧需自行判空
                jsResponse = QJSValue(QJSValue::NullValue);
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

// 统一发送 HTTP 请求的实现（std::function 回调版本），retryCount 用于重试控制
void HttpGoCookApi::sendRequest(QNetworkAccessManager::Operation op,
                                const QString &endpoint,
                                const QVariantMap &data,
                                std::function<void(bool, const QString&, const QJsonDocument&)> callback,
                                int retryCount)
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
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, op, endpoint, data, retryCount]() {
        // 请求完成后自动删除 reply 对象
        reply->deleteLater();

        // 获取 HTTP 状态码
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 如果是 401，发射未授权信号，并触发抽象层回调
        if (statusCode == 401) {
            emit unauthorized();
            invokeUnauthorizedHandler();
        }

        // 如果有网络错误，发射错误信号并回调失败
        if (reply->error() != QNetworkReply::NoError) {
            // 检查是否需要重试：仅对 GET 操作，且重试计数未达上限
            if (op == QNetworkAccessManager::GetOperation && retryCount < m_maxRetries) {
                QTimer::singleShot(m_retryDelay, this, [this, op, endpoint, data, callback, retryCount]() {
                    sendRequest(op, endpoint, data, callback, retryCount + 1);
                });
                return;
            }

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
            callback(false, err.isEmpty() ? "Unknown error" : err.toStdString());
        }
    });
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
                     errorMsg.isEmpty() ? "Unknown error" : errorMsg.toStdString());
            return;
        }
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            gocook::models::LoginResponse resp;
            resp.token = obj["token"].toString().toStdString();
            resp.user_id = obj["userId"].toInt();
            resp.username = obj["username"].toString().toStdString();
            callback(true, resp, "");
        } else {
            callback(false, gocook::models::LoginResponse{}, "Invalid response format");
        }
    });
}

void HttpGoCookApi::forgotPassword(const std::string& email,
                                   SuccessCallback callback)
{
    Q_UNUSED(email);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::resetPassword(const std::string& token,
                                  const std::string& newPassword,
                                  SuccessCallback callback)
{
    Q_UNUSED(token);
    Q_UNUSED(newPassword);
    if (callback) callback(false, "Not implemented");
}

// ======================= 用户相关 =======================
void HttpGoCookApi::getCurrentUser(UserProfileCallback callback) {
    if (callback) callback(false, gocook::models::UserProfile{}, "Not implemented");
}

void HttpGoCookApi::updateProfile(const gocook::models::UpdateProfileRequest& profile,
                                  UserProfileCallback callback) {
    Q_UNUSED(profile);
    if (callback) callback(false, gocook::models::UserProfile{}, "Not implemented");
}

void HttpGoCookApi::getPreferences(PreferencesCallback callback) {
    if (callback) callback(false, gocook::models::UserPreferences{}, "Not implemented");
}

void HttpGoCookApi::updatePreferences(const gocook::models::UserPreferences& prefs,
                                      SuccessCallback callback) {
    Q_UNUSED(prefs);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::updateHealthProfile(const gocook::models::HealthProfileRequest& healthProfile,
                                        HealthProfileCallback callback) {
    Q_UNUSED(healthProfile);
    if (callback) callback(false, gocook::models::HealthProfileResponse{}, "Not implemented");
}

void HttpGoCookApi::uploadAvatar(const std::string& filePath,
                                 AvatarUploadCallback callback)
{
    Q_UNUSED(filePath);
    if (callback) callback(false, gocook::models::AvatarUploadResponse{}, "Not implemented");
}

void HttpGoCookApi::changePassword(const std::string& currentPassword,
                                   const std::string& newPassword,
                                   SuccessCallback callback)
{
    Q_UNUSED(currentPassword);
    Q_UNUSED(newPassword);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::deleteAccount(SuccessCallback callback)
{
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::getFavorites(int page, int size,
                                 const std::string& group,
                                 FavoritesCallback callback) {
    Q_UNUSED(page); Q_UNUSED(size); Q_UNUSED(group);
    if (callback) callback(false, gocook::models::PagedFavorites{}, "Not implemented");
}

void HttpGoCookApi::getFavoriteGroups(FavoriteGroupsCallback callback)
{
    if (callback) callback(false, std::vector<gocook::models::FavoriteGroup>{}, "Not implemented");
}

void HttpGoCookApi::createFavoriteGroup(const gocook::models::CreateGroupRequest& request,
                                        FavoriteGroupCallback callback)
{
    Q_UNUSED(request);
    if (callback) callback(false, gocook::models::FavoriteGroup{}, "Not implemented");
}

void HttpGoCookApi::updateFavoriteGroup(int groupId,
                                        const gocook::models::UpdateGroupRequest& request,
                                        SuccessCallback callback)
{
    Q_UNUSED(groupId);
    Q_UNUSED(request);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::deleteFavoriteGroup(int groupId,
                                        SuccessCallback callback)
{
    Q_UNUSED(groupId);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::updateFavoriteItem(int favoriteId,
                                       const gocook::models::UpdateFavoriteRequest& request,
                                       SuccessCallback callback)
{
    Q_UNUSED(favoriteId);
    Q_UNUSED(request);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::batchDeleteFavorites(const gocook::models::BatchDeleteFavoritesRequest& request,
                                         SuccessCallback callback)
{
    Q_UNUSED(request);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::getNotifications(int page, int size,
                                     const std::string& type,
                                     PagedNotificationsCallback callback)
{
    Q_UNUSED(page);
    Q_UNUSED(size);
    Q_UNUSED(type);
    if (callback) callback(false, gocook::models::PagedNotifications{}, "Not implemented");
}

void HttpGoCookApi::markNotificationRead(int notificationId,
                                         SuccessCallback callback)
{
    Q_UNUSED(notificationId);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::markAllNotificationsRead(SuccessCallback callback)
{
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::deleteNotification(int notificationId,
                                        SuccessCallback callback)
{
    Q_UNUSED(notificationId);
    if (callback) callback(false, "Not implemented");
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
                     errorMsg.isEmpty() ? "Unknown error" : errorMsg.toStdString());
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
    });
}

void HttpGoCookApi::getRecommendedRecipes(int page, int size,
                                          PagedRecommendedRecipesCallback callback) {
    Q_UNUSED(page); Q_UNUSED(size);
    if (callback) callback(false, gocook::models::PagedRecommendedRecipes{}, "Not implemented");
}

void HttpGoCookApi::searchRecipes(const std::string& keyword,
                                  int page, int size,
                                  const nlohmann::json& filters,
                                  PagedRecipesCallback callback) {
    Q_UNUSED(keyword); Q_UNUSED(page); Q_UNUSED(size); Q_UNUSED(filters);
    if (callback) callback(false, gocook::models::PagedRecipes{}, "Not implemented");
}

void HttpGoCookApi::getRecipeDetail(int recipeId,
                                    RecipeDetailCallback callback) {
    Q_UNUSED(recipeId);
    if (callback) callback(false, gocook::models::RecipeDetail{}, "Not implemented");
}

void HttpGoCookApi::getRecipeVideos(int recipeId,
                                    RecipeVideosCallback callback) {
    Q_UNUSED(recipeId);
    if (callback) callback(false, {}, "Not implemented");
}

void HttpGoCookApi::getRecipeRatings(int recipeId, int page, int size,
                                     PagedRatingsCallback callback) {
    Q_UNUSED(recipeId); Q_UNUSED(page); Q_UNUSED(size);
    if (callback) callback(false, gocook::models::PagedRatings{}, "Not implemented");
}

void HttpGoCookApi::submitRecipe(const gocook::models::SubmitRecipeRequest& recipeData,
                                 SubmitRecipeCallback callback) {
    Q_UNUSED(recipeData);
    if (callback) callback(false, gocook::models::SubmitRecipeResponse{}, "Not implemented");
}

void HttpGoCookApi::getMySubmittedRecipes(int page, int size,
                                          const std::string& status,
                                          PagedMyRecipesCallback callback) {
    Q_UNUSED(page); Q_UNUSED(size); Q_UNUSED(status);
    if (callback) callback(false, gocook::models::PagedMyRecipes{}, "Not implemented");
}

void HttpGoCookApi::editRecipe(int recipeId,
                               const gocook::models::EditRecipeRequest& updates,
                               SuccessCallback callback) {
    Q_UNUSED(recipeId); Q_UNUSED(updates);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::toggleFavorite(int recipeId,
                                   std::optional<int> groupId,
                                   std::optional<bool> isPublic,
                                   SuccessCallback callback) {
    Q_UNUSED(recipeId); Q_UNUSED(groupId); Q_UNUSED(isPublic);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::rateRecipe(int recipeId,
                               const gocook::models::RateRecipeRequest& request,
                               SuccessCallback callback) {
    Q_UNUSED(recipeId); Q_UNUSED(request);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::updateRating(int recipeId, int ratingId,
                                  const gocook::models::RateRecipeRequest& request,
                                  SuccessCallback callback)
{
    Q_UNUSED(recipeId);
    Q_UNUSED(ratingId);
    Q_UNUSED(request);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::deleteRating(int recipeId, int ratingId,
                                  SuccessCallback callback)
{
    Q_UNUSED(recipeId);
    Q_UNUSED(ratingId);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::getMyRatings(int page, int size,
                                  PagedUserRatingsCallback callback)
{
    Q_UNUSED(page);
    Q_UNUSED(size);
    if (callback) callback(false, gocook::models::PagedUserRatings{}, "Not implemented");
}

void HttpGoCookApi::getRecipeNutrition(int recipeId,
                                        NutritionReportCallback callback)
{
    Q_UNUSED(recipeId);
    if (callback) callback(false, gocook::models::NutritionReport{}, "Not implemented");
}

// ======================= 库存管理 =======================
void HttpGoCookApi::getInventory(int page, int size,
                                 PagedInventoryCallback callback) {
    Q_UNUSED(page); Q_UNUSED(size);
    if (callback) callback(false, gocook::models::PagedInventory{}, "Not implemented");
}

void HttpGoCookApi::upsertInventory(const gocook::models::UpsertInventoryRequest& item,
                                    IntCallback callback) {
    Q_UNUSED(item);
    if (callback) callback(false, 0, "Not implemented");
}

void HttpGoCookApi::deleteInventoryItem(int itemId,
                                        SuccessCallback callback) {
    Q_UNUSED(itemId);
    if (callback) callback(false, "Not implemented");
}

// ======================= 购物清单 / 膳食计划 =======================
void HttpGoCookApi::getShoppingLists(ShoppingListsCallback callback)
{
    if (callback) callback(false, std::vector<gocook::models::ShoppingListSummary>{}, "Not implemented");
}

void HttpGoCookApi::createShoppingList(const gocook::models::CreateShoppingListRequest& request,
                                        ShoppingListCallback callback)
{
    Q_UNUSED(request);
    if (callback) callback(false, gocook::models::ShoppingList{}, "Not implemented");
}

void HttpGoCookApi::getShoppingListDetail(int listId,
                                           ShoppingListCallback callback)
{
    Q_UNUSED(listId);
    if (callback) callback(false, gocook::models::ShoppingList{}, "Not implemented");
}

void HttpGoCookApi::deleteShoppingList(int listId,
                                        SuccessCallback callback)
{
    Q_UNUSED(listId);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::updateShoppingListItem(int listId, int itemId,
                                           const gocook::models::UpdateShoppingItemRequest& request,
                                           SuccessCallback callback) {
    Q_UNUSED(listId); Q_UNUSED(itemId); Q_UNUSED(request);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::batchAddShoppingItems(int listId,
                                          const std::vector<gocook::models::BatchShoppingItem>& items,
                                          BatchShoppingCallback callback) {
    Q_UNUSED(listId); Q_UNUSED(items);
    if (callback) callback(false, gocook::models::BatchShoppingResponse{}, "Not implemented");
}

void HttpGoCookApi::exportShoppingList(int listId,
                                        const std::string& format,
                                        std::function<void(bool, const std::string&, const std::string&)> callback)
{
    Q_UNUSED(listId);
    Q_UNUSED(format);
    if (callback) callback(false, "", "Not implemented");
}

// ======================= 膳食计划 =======================
void HttpGoCookApi::createMealPlan(const gocook::models::MealPlanRequest& planData,
                                   IntCallback callback) {
    Q_UNUSED(planData);
    if (callback) callback(false, 0, "Not implemented");
}

void HttpGoCookApi::getMealPlans(const std::string& startDate,
                                 const std::string& endDate,
                                 int page, int size,
                                 MealPlansCallback callback) {
    Q_UNUSED(startDate); Q_UNUSED(endDate); Q_UNUSED(page); Q_UNUSED(size);
    if (callback) callback(false, gocook::models::MealPlansResponse{}, "Not implemented");
}

void HttpGoCookApi::getMealPlanDetail(const std::string& startDate,
                                      const std::string& endDate,
                                      int page, int size,
                                      MealPlanCalendarCallback callback) {
    Q_UNUSED(startDate); Q_UNUSED(endDate); Q_UNUSED(page); Q_UNUSED(size);
    if (callback) callback(false, gocook::models::PagedCalendarDays{}, "Not implemented");
}

void HttpGoCookApi::updateMealPlan(int planId,
                                   const gocook::models::MealPlanRequest& updates,
                                   SuccessCallback callback) {
    Q_UNUSED(planId); Q_UNUSED(updates);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::deleteMealPlan(int planId,
                                   SuccessCallback callback) {
    Q_UNUSED(planId);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::getNutritionTrend(const std::string& startDate,
                                      const std::string& endDate,
                                      NutritionTrendCallback callback) {
    Q_UNUSED(startDate); Q_UNUSED(endDate);
    if (callback) callback(false, gocook::models::NutritionTrendResponse{}, "Not implemented");
}

// ======================= 公告 =======================
void HttpGoCookApi::getAnnouncements(int page, int size,
                                     PagedAnnouncementsCallback callback) {
    Q_UNUSED(page); Q_UNUSED(size);
    if (callback) callback(false, gocook::models::PagedAnnouncements{}, "Not implemented");
}

// ======================= 管理员功能 =======================
void HttpGoCookApi::getUsers(int page, int size,
                             const nlohmann::json& filters,
                             PagedUsersCallback callback) {
    Q_UNUSED(page); Q_UNUSED(size); Q_UNUSED(filters);
    if (callback) callback(false, gocook::models::PagedUsers{}, "Not implemented");
}

void HttpGoCookApi::createUser(const gocook::models::CreateUserRequest& userData,
                               SuccessCallback callback) {
    Q_UNUSED(userData);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::updateUser(int userId,
                               const gocook::models::UpdateUserRequest& updates,
                               SuccessCallback callback) {
    Q_UNUSED(userId); Q_UNUSED(updates);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::setUserStatus(int userId,
                                  const gocook::models::SetUserStatusRequest& request,
                                  SuccessCallback callback) {
    Q_UNUSED(userId); Q_UNUSED(request);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::deleteUser(int userId,
                               SuccessCallback callback) {
    Q_UNUSED(userId);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::getPendingRecipes(int page, int size,
                                      PagedPendingRecipesCallback callback) {
    Q_UNUSED(page); Q_UNUSED(size);
    if (callback) callback(false, gocook::models::PagedPendingRecipes{}, "Not implemented");
}

void HttpGoCookApi::approveRecipe(int recipeId,
                                  SuccessCallback callback) {
    Q_UNUSED(recipeId);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::rejectRecipe(int recipeId,
                                 const gocook::models::RejectRecipeRequest& request,
                                 SuccessCallback callback) {
    Q_UNUSED(recipeId); Q_UNUSED(request);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::batchReviewRecipes(const gocook::models::BatchReviewRequest& request,
                                       BatchReviewCallback callback) {
    Q_UNUSED(request);
    if (callback) callback(false, gocook::models::BatchReviewResponse{}, "Not implemented");
}

void HttpGoCookApi::publishAnnouncement(const gocook::models::AnnouncementRequest& request,
                                        SuccessCallback callback) {
    Q_UNUSED(request);
    if (callback) callback(false, "Not implemented");
}

void HttpGoCookApi::sendNotification(const gocook::models::NotificationRequest& notification,
                                     NotificationCallback callback) {
    Q_UNUSED(notification);
    if (callback) callback(false, gocook::models::NotificationResponse{}, "Not implemented");
}

void HttpGoCookApi::getStatistics(StatisticsCallback callback)
{
    if (callback) callback(false, gocook::models::StatisticsData{}, "Not implemented");
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
    if (callback) callback(false, gocook::models::PagedAdminLogs{}, "Not implemented");
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
    if (callback) callback(false, gocook::models::PagedActivityLogs{}, "Not implemented");
}

// ======================= 令牌管理 =======================
void HttpGoCookApi::setAuthToken(const std::string& token) {
    setToken(QString::fromStdString(token));
}

std::string HttpGoCookApi::authToken() const {
    return token().toStdString();
}