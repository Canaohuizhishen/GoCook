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

// 统一发送 HTTP 请求的实现（QJSValue 回调版本），委托给 std::function 版本
void HttpGoCookApi::sendRequest(QNetworkAccessManager::Operation op,
                                const QString &endpoint,
                                const QVariantMap &data,
                                const QJSValue &callback,
                                int retryCount)
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
    sendRequest(op, endpoint, data, wrapped, retryCount);
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

    // 连接请求完成信号，QPointer 守卫防止对象销毁后 λ 访问已释放内存
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, op, endpoint, data, retryCount, self = QPointer<HttpGoCookApi>(this)]() {
        if (!self) return;

        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 401 时仍要读取响应体并通过 callback 返回错误信息
        if (statusCode == 401) {
            emit unauthorized();
            invokeUnauthorizedHandler();
            QByteArray responseData = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            reply->deleteLater();
            if (callback) {
                callback(false, QString::fromUtf8(responseData), doc);
            }
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            if (op == QNetworkAccessManager::GetOperation && retryCount < m_maxRetries) {
                QTimer::singleShot(m_retryDelay, this, [self, op, endpoint, data, callback, retryCount]() {
                    if (!self) return;
                    self->sendRequest(op, endpoint, data, callback, retryCount + 1);
                });
                reply->deleteLater();
                return;
            }

            emit networkError(reply->errorString());
            if (callback) {
                callback(false, reply->errorString(), QJsonDocument());
            }
            reply->deleteLater();
            return;
        }

        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        bool success = (statusCode >= 200 && statusCode < 300);

        if (callback) {
            callback(success, success ? QString() : QString::fromUtf8(responseData), doc);
        }
        reply->deleteLater();
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
            resp.user_id = obj["user_id"].toInt();
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
    });
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
            for (const QJsonValue& val : dataArr)
                result.data.push_back(parseRecipeSummary(val.toObject()));
        }
        callback(true, result, "");
    });
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
                detail.steps.push_back(step);
            }
        }
        if (obj.contains("nutrition") && obj["nutrition"].isObject()) {
            QJsonObject nut = obj["nutrition"].toObject();
            detail.nutrition.calories = nut["calories"].toDouble();
            detail.nutrition.protein = nut["protein"].toDouble();
            detail.nutrition.fat = nut["fat"].toDouble();
            detail.nutrition.carbs = nut["carbs"].toDouble();
        }
        if (obj.contains("tags") && obj["tags"].isArray()) {
            for (const auto& val : obj["tags"].toArray())
                detail.tags.push_back(val.toString().toStdString());
        }
        detail.author_id = obj["author_id"].toInt();
        detail.author_name = obj["author_name"].toString().toStdString();
        detail.created_at = obj["created_at"].toString().toStdString();
        callback(true, detail, "");
    });
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
    });
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
    });
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
    });
}

void HttpGoCookApi::deleteInventoryItem(int itemId,
                                        SuccessCallback callback) {
    QString endpoint = QString("/api/inventory/%1").arg(itemId);
    deleteResource(endpoint, {}, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        callback(success, success ? "" : errorMsg.toStdString());
    });
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
    });
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
    });
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
    });
}

void HttpGoCookApi::deleteShoppingList(int listId,
                                        SuccessCallback callback)
{
    QString endpoint = QString("/api/inventory/shopping-lists/%1").arg(listId);
    deleteResource(endpoint, {}, [callback](bool success, const QString& errorMsg, const QJsonDocument&) {
        if (callback) callback(success, success ? "" : errorMsg.toStdString());
    });
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

    QNetworkReply* reply = m_nam.sendCustomRequest(req, "PATCH", payload);

    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        if (statusCode == 200) {
            callback(true, "");
        } else {
            QByteArray responseData = reply->readAll();
            callback(false, QString::fromUtf8(responseData).toStdString());
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

    QNetworkReply* reply = m_nam.post(request, body);

    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray responseData = reply->readAll();
        reply->deleteLater();

        if (statusCode == 201) {
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
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
            callback(true, resp, "");
        } else {
            callback(false, gocook::models::BatchShoppingResponse{}, QString::fromUtf8(responseData).toStdString());
        }
    });
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
    });
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