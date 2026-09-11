#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <functional>
#include <vector>
#include <QJSValue>
#include <QTimer>
#include <gocook/IGoCookApi.h>

/**
 * @brief 网络请求客户端类，封装 QNetworkAccessManager 提供统一的 REST API 调用接口
 *
 * 支持 GET、POST、PUT、DELETE 四种 HTTP 方法，自动携带 Bearer Token 认证头。
 * 回调函数使用 QJSValue，可在 QML 中直接传入 JavaScript 函数。
 * 同时提供 std::function 回调重载，便于 C++ 内部调用。
 * 当收到 401 未授权响应时，会发出 unauthorized 信号，便于上层跳转登录。
 * 支持设置最大重试次数和重试延迟，对 GET 请求在网络错误时自动重试。
 */
class HttpGoCookApi : public QObject, public IGoCookApi
{
    Q_OBJECT
    // 暴露给 QML 的属性：基础 URL
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    // 暴露给 QML 的属性：认证令牌
    Q_PROPERTY(QString token READ token WRITE setToken NOTIFY tokenChanged)
    // 暴露给 QML 的属性：最大重试次数（0 表示不重试，仅对 GET 请求有效）
    Q_PROPERTY(int maxRetries READ maxRetries WRITE setMaxRetries NOTIFY maxRetriesChanged)
    // 暴露给 QML 的属性：重试间隔（毫秒）
    Q_PROPERTY(int retryDelay READ retryDelay WRITE setRetryDelay NOTIFY retryDelayChanged)

public:
    // 接口鉴权模式（每个 API 方法在调用发送门面时声明）：
    //   Public      游客可调（服务端公开接口），直接发送
    //   Silent      需登录；未登录时不发送请求、回调失败（GET 加载类，页面呈现"登录后可用"空态）
    //   Interactive 需登录；未登录时挂起请求并弹应用内登录页，登录成功后自动重放（用户主动写操作）
    enum class AuthMode { Public, Silent, Interactive };
    // Q_ENUM 注册：QML 将来若直接给 get/post/... 传鉴权模式参数，可用 HttpGoCookApi.AuthMode.XXX 或字符串转换
    Q_ENUM(AuthMode)

    // 登录守卫拦截的统一失败文案（未登录且接口需登录，请求未发出）：
    // 唯一出口是 errorMessageFor(-2)，VM 层用此常量区分"守卫拦截"与"网络/服务器错误"，
    // 避免魔法字符串在多个文件间散落漂移（文案变更只改这一处；QML 侧比较见 InventoryPage）
    inline static const QString kAuthRequiredError = QStringLiteral("请先登录");

    // 构造函数
    explicit HttpGoCookApi(QObject *parent = nullptr);

    // 获取当前基础 URL
    QString baseUrl() const { return m_baseUrl; }
    // 设置基础 URL
    void setBaseUrl(const QString &url);

    // 获取当前令牌
    QString token() const { return m_token; }
    // 设置认证令牌
    void setToken(const QString &token);

    // 获取最大重试次数
    int maxRetries() const { return m_maxRetries; }
    // 设置最大重试次数
    void setMaxRetries(int retries);

    // 获取重试延迟（毫秒）
    int retryDelay() const { return m_retryDelay; }
    // 设置重试延迟（毫秒）
    void setRetryDelay(int delayMs);

    // 供 QML 调用的 GET 请求方法
    Q_INVOKABLE void get(const QString &endpoint, const QJSValue &callback,
                         AuthMode authMode = AuthMode::Public);
    // 供 C++ 调用的 GET 请求方法
    // suppressNetworkError=true：失败时不发全局 networkError（页面自行呈现离线状态，如详情缓存兜底）
    void get(const QString &endpoint,
             std::function<void(bool, const QString&, const QJsonDocument&)> callback,
             bool suppressNetworkError = false,
             AuthMode authMode = AuthMode::Public);
    // 供 QML 调用的 POST 请求方法
    Q_INVOKABLE void post(const QString &endpoint, const QVariantMap &data, const QJSValue &callback,
                          AuthMode authMode = AuthMode::Public);
    // 供 C++ 调用的 POST 请求方法（使用 std::function 回调）
    void post(const QString &endpoint, const QVariantMap &data,
              std::function<void(bool, const QString&, const QJsonDocument&)> callback,
              bool suppressNetworkError = false,
              AuthMode authMode = AuthMode::Public);
    // 供 QML 调用的 DELETE 请求方法
    Q_INVOKABLE void deleteResource(const QString &endpoint, const QVariantMap &data, const QJSValue &callback,
                                    AuthMode authMode = AuthMode::Public);
    // 供 C++ 调用的 DELETE 请求方法
    void deleteResource(const QString &endpoint, const QVariantMap &data,
                        std::function<void(bool, const QString&, const QJsonDocument&)> callback,
                        bool suppressNetworkError = false,
                        AuthMode authMode = AuthMode::Public);
    // 供 QML 调用的 PUT 请求方法
    Q_INVOKABLE void put(const QString &endpoint, const QVariantMap &data, const QJSValue &callback,
                         AuthMode authMode = AuthMode::Public);
    // 供 C++ 调用的 PUT 请求方法
    void put(const QString &endpoint, const QVariantMap &data,
             std::function<void(bool, const QString&, const QJsonDocument&)> callback,
             bool suppressNetworkError = false,
             AuthMode authMode = AuthMode::Public);
    // 供 QML 调用的 PATCH 请求方法
    Q_INVOKABLE void patch(const QString &endpoint, const QVariantMap &data, const QJSValue &callback,
                           AuthMode authMode = AuthMode::Public);
    // 供 C++ 调用的 PATCH 请求方法
    void patch(const QString &endpoint, const QVariantMap &data,
               std::function<void(bool, const QString&, const QJsonDocument&)> callback,
               bool suppressNetworkError = false,
               AuthMode authMode = AuthMode::Public);

    // ---------- 实现 GoCookApi 抽象接口 ----------
    // 认证
    void registerUser(const gocook::models::RegisterRequest& request,
                      SuccessCallback callback) override;
    void verifyRegistration(const std::string& email,
                            const std::string& token,
                            SuccessCallback callback) override;
    void login(const gocook::models::LoginRequest& request,
               LoginCallback callback) override;
    void forgotPassword(const std::string& username,
                        const std::string& email,
                        SuccessCallback callback) override;
    void resetPassword(const std::string& token,
                       const std::string& newPassword,
                       SuccessCallback callback) override;

    // 用户相关
    void getCurrentUser(UserProfileCallback callback) override;
    void updateProfile(const gocook::models::UpdateProfileRequest& profile,
                       UserProfileCallback callback) override;
    void getPreferences(PreferencesCallback callback) override;
    void updatePreferences(const gocook::models::UserPreferences& prefs,
                           SuccessCallback callback) override;
    void updateHealthProfile(const gocook::models::HealthProfileRequest& healthProfile,
                             HealthProfileCallback callback) override;
    void getHealthProfile(HealthProfileCallback callback) override;
    void uploadAvatar(const std::string& filePath,
                      AvatarUploadCallback callback) override;
    void uploadRecipeImage(int recipeId,
                           const std::string& filePath,
                           RecipeImageCallback callback) override;
    void uploadStepImage(int recipeId, int stepIndex,
                         const std::string& filePath,
                         RecipeImageCallback callback) override;
    void deleteRecipe(int recipeId, SuccessCallback callback) override;
    void changePassword(const std::string& currentPassword,
                        const std::string& newPassword,
                        SuccessCallback callback) override;
    void deleteAccount(SuccessCallback callback) override;
    void getFavorites(int page, int size,
                      const std::string& group,
                      FavoritesCallback callback) override;
    void getFavoriteGroups(FavoriteGroupsCallback callback) override;
    void createFavoriteGroup(const gocook::models::CreateGroupRequest& request,
                             FavoriteGroupCallback callback) override;
    void updateFavoriteGroup(int groupId,
                             const gocook::models::UpdateGroupRequest& request,
                             SuccessCallback callback) override;
    void deleteFavoriteGroup(int groupId,
                             SuccessCallback callback) override;
    void updateFavoriteItem(int favoriteId,
                            const gocook::models::UpdateFavoriteRequest& request,
                            SuccessCallback callback) override;
    void batchDeleteFavorites(const gocook::models::BatchDeleteFavoritesRequest& request,
                              SuccessCallback callback) override;
    void getNotifications(int page, int size,
                          const std::string& type,
                          PagedNotificationsCallback callback) override;
    void markNotificationRead(int notificationId,
                              SuccessCallback callback) override;
    void markAllNotificationsRead(SuccessCallback callback) override;
    void deleteNotification(int notificationId,
                            SuccessCallback callback) override;

    // 菜谱相关
    void getPublicRecipes(int page, int size,
                          const nlohmann::json& filters,
                          PagedRecipesCallback callback) override;
    void getRecommendedRecipes(int page, int size,
                               PagedRecommendedRecipesCallback callback) override;
    void searchRecipes(const std::string& keyword,
                       int page, int size,
                       const nlohmann::json& filters,
                       PagedRecipesCallback callback) override;
    void getRecipeDetail(int recipeId,
                         RecipeDetailCallback callback) override;
    void getRecipeVideos(int recipeId,
                         RecipeVideosCallback callback) override;
    void getRecipeRatings(int recipeId, int page, int size,
                          PagedRatingsCallback callback) override;
    void getMyRecipeRating(int recipeId,
                            MyRecipeRatingCallback callback) override;
    void submitRecipe(const gocook::models::SubmitRecipeRequest& recipeData,
                      SubmitRecipeCallback callback) override;
    void getMySubmittedRecipes(int page, int size,
                               const std::string& status,
                               PagedMyRecipesCallback callback) override;
    void editRecipe(int recipeId,
                    const gocook::models::EditRecipeRequest& updates,
                    SuccessCallback callback) override;
    void toggleFavorite(int recipeId,
                        std::optional<int> groupId,
                        std::optional<bool> isPublic,
                        SuccessCallback callback) override;
    void rateRecipe(int recipeId,
                    const gocook::models::RateRecipeRequest& request,
                    SuccessCallback callback) override;
    void updateRating(int recipeId, int ratingId,
                      const gocook::models::RateRecipeRequest& request,
                      SuccessCallback callback) override;
    void deleteRating(int recipeId, int ratingId,
                      SuccessCallback callback) override;
    void getMyRatings(int page, int size,
                      PagedUserRatingsCallback callback) override;
    void getRecipeNutrition(int recipeId,
                            NutritionReportCallback callback) override;

    // 库存管理
    void getInventory(int page, int size, const std::string& keyword,
                      PagedInventoryCallback callback) override;
    void upsertInventory(const gocook::models::UpsertInventoryRequest& item,
                         IntCallback callback) override;
    void updateInventoryItem(int itemId,
                             const gocook::models::UpsertInventoryRequest& item,
                             SuccessCallback callback) override;
    void deleteInventoryItem(int itemId,
                             SuccessCallback callback) override;

    // 购物清单 / 膳食计划
    void getShoppingLists(ShoppingListsCallback callback) override;
    void createShoppingList(const gocook::models::CreateShoppingListRequest& request,
                            ShoppingListCallback callback) override;
    void getShoppingListDetail(int listId,
                               ShoppingListCallback callback) override;
    void deleteShoppingList(int listId,
                            SuccessCallback callback) override;
    void updateShoppingListItem(int listId, int itemId,
                                const gocook::models::UpdateShoppingItemRequest& request,
                                SuccessCallback callback) override;
    void batchAddShoppingItems(int listId,
                               const std::vector<gocook::models::BatchShoppingItem>& items,
                               BatchShoppingCallback callback) override;
    void exportShoppingList(int listId,
                            const std::string& format,
                            std::function<void(bool, const std::string&, const std::string&)> callback) override;

    // 膳食计划
    void createMealPlan(const gocook::models::MealPlanRequest& planData,
                        IntCallback callback) override;
    void getMealPlans(const std::string& startDate,
                      const std::string& endDate,
                      int page, int size,
                      MealPlansCallback callback) override;
    void getMealPlanDetail(const std::string& startDate,
                           const std::string& endDate,
                           int page, int size,
                           MealPlanCalendarCallback callback) override;
    void updateMealPlan(int planId,
                        const gocook::models::MealPlanRequest& updates,
                        SuccessCallback callback) override;
    void deleteMealPlan(int planId,
                        SuccessCallback callback) override;
    void getNutritionTrend(const std::string& startDate,
                           const std::string& endDate,
                           NutritionTrendCallback callback) override;

    // 公告
    void getAnnouncements(int page, int size,
                          PagedAnnouncementsCallback callback) override;

    // 管理员功能
    void getUsers(int page, int size,
                  const nlohmann::json& filters,
                  PagedUsersCallback callback) override;
    void createUser(const gocook::models::CreateUserRequest& userData,
                    SuccessCallback callback) override;
    void updateUser(int userId,
                    const gocook::models::UpdateUserRequest& updates,
                    SuccessCallback callback) override;
    void setUserStatus(int userId,
                       const gocook::models::SetUserStatusRequest& request,
                       SuccessCallback callback) override;
    void deleteUser(int userId,
                    SuccessCallback callback) override;
    void getPendingRecipes(int page, int size,
                           PagedPendingRecipesCallback callback) override;
    void approveRecipe(int recipeId,
                       SuccessCallback callback) override;
    void rejectRecipe(int recipeId,
                      const gocook::models::RejectRecipeRequest& request,
                      SuccessCallback callback) override;
    void batchReviewRecipes(const gocook::models::BatchReviewRequest& request,
                            BatchReviewCallback callback) override;
    void publishAnnouncement(const gocook::models::AnnouncementRequest& request,
                             SuccessCallback callback) override;
    void sendNotification(const gocook::models::NotificationRequest& notification,
                          NotificationCallback callback) override;
    void getStatistics(StatisticsCallback callback) override;
    void resetTestNotifications(SuccessCallback callback) override;
    void getAdminLogs(int page, int size,
                      const std::string& type,
                      int userId,
                      PagedAdminLogsCallback callback) override;
    void getActivityLogs(int page, int size,
                         int userId,
                         const std::string& action,
                         PagedActivityLogsCallback callback) override;

    // 令牌管理
    void setAuthToken(const std::string& token) override;
    std::string authToken() const override;

    // 注册未授权回调（响应 401 时自动触发）
    void setUnauthorizedHandler(std::function<void()> handler) {
        m_unauthorizedHandler = std::move(handler);
    }

    // 取消全部挂起的 Interactive 请求（应用内登录页被关闭/跳过时调用）：
    // 每个挂起请求按"请先登录"回调失败，不再重放
    Q_INVOKABLE void cancelAuthQueue();

    // 供子类/自身调用，触发未授权回调
    void invokeUnauthorizedHandler() {
        if (m_unauthorizedHandler) {
            m_unauthorizedHandler();
        }
    }

signals:
    // 基础 URL 变更信号
    void baseUrlChanged();
    // 令牌变更信号
    void tokenChanged();
    // 网络错误信号，携带错误描述
    void networkError(const QString &errorString);
    // 未授权信号（401），用于触发跳转登录
    void unauthorized();
    // 登录守卫信号：存在未登录时被挂起的 Interactive 请求，
    // 上层应弹出应用内登录页；登录成功后请求自动重放，登录页可关闭（取消则调 cancelAuthQueue）
    void authRequired();
    // 最大重试次数变更信号
    void maxRetriesChanged();
    // 重试延迟变更信号
    void retryDelayChanged();

private:
    // 构造标准 JSON 请求（URL、超时、Content-Type、Authorization），供 sendRequest 使用
    QNetworkRequest buildRequest(const QString &endpoint, const QString &methodOverride = "");
    // 统一底层发送原语：所有 HTTP 请求（含文件上传等手写路径）的唯一出口。
    // 发送前做登录守卫 preflight：未登录且 authMode != Public 时不发送请求，
    // Interactive 挂起入队并 emit authRequired()（登录成功后按入队顺序重放，取消时按 -2 回调失败）；
    // Silent 直接回调 handler(-2, 空)（-2 = 被登录守卫拦截，调用方按"请先登录"处理）。
    // 发送后统一读取 statusCode/响应体并 deleteLater；401 统一发 unauthorized + invokeUnauthorizedHandler；
    // 然后调用 handler(statusCode, responseData)：statusCode<=0 表示网络层错误（无服务端响应）。
    // 该函数不重试、不做业务解析——重试与回调转换由调用方（sendRequest 等）负责。
    void sendRaw(AuthMode authMode,
                 QNetworkAccessManager::Operation op,
                 QNetworkRequest &request,
                 const QByteArray &body,
                 const QString &methodOverride,
                 std::function<void(int statusCode, const QByteArray &responseData)> handler);
    // 统一发送 HTTP 请求的内部方法（用于 QJSValue 回调），增加重试计数参数
    void sendRequest(QNetworkAccessManager::Operation op,
                     const QString &endpoint,
                     const QVariantMap &data,
                     const QJSValue &callback,
                     int retryCount = 0,
                     const QString &methodOverride = "",
                     bool suppressNetworkError = false,
                     AuthMode authMode = AuthMode::Public);
    // 统一发送 HTTP 请求的内部方法（用于 std::function 回调），增加重试计数参数
    void sendRequest(QNetworkAccessManager::Operation op,
                     const QString &endpoint,
                     const QVariantMap &data,
                     std::function<void(bool, const QString&, const QJsonDocument&)> callback,
                     int retryCount = 0,
                     const QString &methodOverride = "",
                     bool suppressNetworkError = false,
                     AuthMode authMode = AuthMode::Public);

    // 一个被登录守卫挂起的 Interactive 请求（登录成功后按入队顺序重放）
    struct PendingAuthRequest {
        QNetworkAccessManager::Operation op;
        QNetworkRequest request;
        QByteArray body;
        QString methodOverride;
        std::function<void(int statusCode, const QByteArray &responseData)> handler;
    };
    // 登录成功后重放全部挂起请求（tokenChanged 触发；重放时 token 已非空，不会再次拦截）
    void replayPendingAuthRequests();
    // 清空挂起队列并让每个请求回调失败（failWith=是否按 -2"请先登录"回调）
    void clearPendingAuthRequests();

    // 网络访问管理器
    QNetworkAccessManager m_nam;
    // 基础 URL
    QString m_baseUrl;
    // 认证令牌
    QString m_token;
    // 挂起的 Interactive 请求队列（登录成功后重放；登录页取消时清空）
    std::vector<PendingAuthRequest> m_pendingAuthRequests;
    // 最大重试次数（仅对 GET 请求生效）
    int m_maxRetries = 0;
    // 重试间隔（毫秒）
    int m_retryDelay = 1000;
    // 未授权回调
    std::function<void()> m_unauthorizedHandler;
};