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
 * @brief IGoCookApi 的 HTTP 实现：客户端所有网络请求由此发出。
 *
 * 双继承：QObject（信号槽 + QML 属性）+ IGoCookApi（ViewModel 经接口注入，如 AuthViewModel）。
 * 支持 GET / POST / PUT / PATCH / DELETE；文件上传、清单导出、清单项批量添加、个别 PATCH 为手写路径。
 * 自动携带 Bearer Token（仅 token 非空时）。
 *
 * 三个设计要点：
 *   1. sendRaw 是唯一网络出口——登录守卫、401 上报、QPointer 防悬垂均在此收口，
 *      手写路径与门面路径行为一致；
 *   2. AuthMode 三档登录守卫（见枚举注释）；
 *   3. 错误文案统一由 errorMessageFor 产出（见 .cpp）；双端门面回调契约见重载区说明。
 *
 * 接口方法语义见 IGoCookApi.h；本文件只补充实现特有契约。
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
    // Q_ENUM 注册（供元对象系统/QML 使用）。若将来 QML 要直接传 AuthMode 参数，
    // 需先把类型/枚举报进 QML 类型系统；当前 QML 调用方均不传该参数，
    // 鉴权模式由各接口方法在 .cpp 内按接口语义声明。
    Q_ENUM(AuthMode)

    // 登录守卫拦截的统一失败文案（未登录且接口需登录，请求未发出）：
    // 唯一出口是 errorMessageFor(-2)，VM 层用此常量区分"守卫拦截"与"网络/服务器错误"，
    // 避免魔法字符串在多个文件间散落漂移（文案变更只改这一处；QML 侧比较见 InventoryPage）
    inline static const QString kAuthRequiredError = QStringLiteral("请先登录");

    explicit HttpGoCookApi(QObject *parent = nullptr);

    // 基础 URL（默认 "http://127.0.0.1:8080"，构造函数中设置）。
    // 契约：形如 scheme://host[:port]，不要以 '/' 结尾——内部按 m_baseUrl + endpoint 直接拼接
    // （endpoint 自带前导 '/'）。不校验合法性；变更时 emit baseUrlChanged()。
    QString baseUrl() const { return m_baseUrl; }
    void setBaseUrl(const QString &url);

    // 认证令牌。变更时 emit tokenChanged() 并联动登录守卫：
    // 置为非空 → 重放挂起的 Interactive 请求；置空 → 挂起请求按 -2 失败作废。
    // 接口调用方走 setAuthToken（见“令牌管理”区）；QML 属性绑定用本组。
    QString token() const { return m_token; }
    void setToken(const QString &token);

    // 最大重试次数（默认 0 = 不重试；仅对 GET、且仅网络层错误生效，见 .cpp sendRequest）。
    // 变更时 emit maxRetriesChanged()。
    int maxRetries() const { return m_maxRetries; }
    void setMaxRetries(int retries);

    // 重试间隔（毫秒，默认 1000）。变更时 emit retryDelayChanged()。
    int retryDelay() const { return m_retryDelay; }
    void setRetryDelay(int delayMs);

    // ============ HTTP 门面重载说明（GET/POST/PUT/PATCH/DELETE 各一对） ============
    // 每个方法两版：QML 版（Q_INVOKABLE + QJSValue）、C++ 版（std::function）。
    // 分两版的原因：QML 与 C++ 是两套类型系统，回调无法互传。
    // 实现关系：C++ 版为主实现；QML 版把 JS 回调包一层适配器后委托 C++ 版（见 .cpp）。
    // 回调契约（两版一致，调用前必读）：三参 (success, errMsg, response)——errMsg 为中文文案，
    // 失败时由 errorMessageFor 产出、成功时为空串；response 为解析后的对象/数组（响应体非 JSON 时为空对象）。
    // 唯一差异：QML 版无 suppressNetworkError 形参，等价固定传 false（除 401/-2 外失败会发全局 networkError）。
    // 现状：QML 侧尚未直接调用这 5 组（仅用信号与 cancelAuthQueue），如新增直调注意上述契约。
    // ============================================================================
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

    // ---------- 实现 IGoCookApi 抽象接口 ----------
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

    // 令牌管理（IGoCookApi 接口入口，AuthViewModel 调用）
    void setAuthToken(const std::string& token) override;   // 等价于 setToken(QString::fromStdString(token))
    std::string authToken() const override;                 // 等价于 token().toStdString()

    // 注册 401 回调（单槽，后注册覆盖前注册；未注册时静默跳过）。
    // 与 unauthorized() 信号的分工：信号=全局广播（当前无生产消费方，保留供上层挂接），
    // 本回调=单一订阅者，典型用途是 AuthViewModel 注入“token 失效 → 自动登出”。触发顺序：先信号，后本回调。
    void setUnauthorizedHandler(std::function<void()> handler) {
        m_unauthorizedHandler = std::move(handler);
    }

    // 取消全部挂起的 Interactive 请求（应用内登录页被关闭/跳过时调用）：
    // 每个挂起请求按"请先登录"回调失败，不再重放
    Q_INVOKABLE void cancelAuthQueue();

    // 触发未授权回调。当前唯一调用点：sendRaw 的 401 分支（.cpp）。
    void invokeUnauthorizedHandler() {
        if (m_unauthorizedHandler) {
            m_unauthorizedHandler();
        }
    }

signals:
    void baseUrlChanged();
    void tokenChanged();
    // 全局请求失败信号（Main.qml 底部 toast 消费），文案为用户可读中文（errorMessageFor 产出）。
    // 仅经 sendRequest 门面路径的请求会发出（手写 sendRaw 路径不发）。触发条件：
    //   1) 网络层错误（statusCode<=0；其中 -1=请求未能发出、不重试直接发出；0 为 GET 时先按 maxRetries 重试，耗尽后才发出）；
    //   2) 服务端返回非 2xx（401 与守卫拦截 -2 除外：401 走 unauthorized，-2 静默）。
    // 注意：业务 4xx（如 400 参数校验失败）同样会触发；
    // C++ 调用方可传 suppressNetworkError=true 抑制（如详情缓存兜底页面）。
    void networkError(const QString &errorString);
    // 未授权信号（401）：全局广播；当前无生产消费方（仅测试连接），
    // 401 兜底实际由 setUnauthorizedHandler 回调承载（AuthViewModel 自动登出）。
    void unauthorized();
    // 登录守卫信号：存在未登录时被挂起的 Interactive 请求，
    // 上层应弹出应用内登录页；登录成功后请求自动重放，登录页可关闭（取消则调 cancelAuthQueue）
    void authRequired();
    void maxRetriesChanged();
    void retryDelayChanged();

private:
    // 构造标准 JSON 请求（URL、超时、Content-Type、Authorization），供 sendRequest 使用
    QNetworkRequest buildRequest(const QString &endpoint);
    // 统一底层发送原语：所有 HTTP 请求（含文件上传、导出等手写路径）的唯一网络出口。
    // 【前置守卫】未登录且 authMode != Public 时绝不发出请求：
    //     Interactive → 挂起入队 + emit authRequired()（登录成功后按入队顺序重放；
    //                   登录页取消或登出时按 -2 回调失败）
    //     Silent      → 立即 handler(-2, {})
    // 【回调契约】handler(statusCode, responseData)，正常情况下恰好回调一次
    //   （被挂起时延迟到重放/取消时回调；handler 可传空，内部判空）：
    //     >0  服务端 HTTP 响应码（含 4xx/5xx）
    //     0   网络层错误（无服务端响应，如断网/超时）
    //     -1  请求未能发出（极端情况）
    //     -2  被登录守卫拦截（未发送任何请求；文案即 kAuthRequiredError）
    //   responseData 为原始响应体，本函数不做 JSON 解析。
    // 【副作用】收到 401：先 emit unauthorized()，再调 m_unauthorizedHandler（若已注册）。
    // 【不做的事】不重试、不做业务解析、不发 networkError——由 sendRequest 门面层负责。
    void sendRaw(AuthMode authMode,
                 QNetworkAccessManager::Operation op,
                 QNetworkRequest &request,
                 const QByteArray &body,
                 const QString &methodOverride,
                 std::function<void(int statusCode, const QByteArray &responseData)> handler);
    // 内部门面（QJSValue 版）：把 JS 回调适配为 std::function 后委托主实现，行为完全一致。
    void sendRequest(QNetworkAccessManager::Operation op,
                     const QString &endpoint,
                     const QVariantMap &data,
                     const QJSValue &callback,
                     int retryCount,
                     const QString &methodOverride,
                     bool suppressNetworkError,
                     AuthMode authMode);
    // 内部门面（主实现）：buildRequest → sendRaw → 回调转换，回调三参 (success, 文案, 响应 JSON)。
    // 转换规则：-2 / -1 / 401 短路失败（401 的 unauthorized 已由 sendRaw 发出；-1 不重试）；
    // 网络层错误（0）仅 GET 按 maxRetries 重试；其余非 2xx 失败并（除非抑制）发 networkError。
    // retryCount：仅重试递归时递增，所有现有调用点均显式传参（门面一律传 0）。
    // methodOverride：仅 CustomOperation（PATCH）有意义，其余传空。
    void sendRequest(QNetworkAccessManager::Operation op,
                     const QString &endpoint,
                     const QVariantMap &data,
                     std::function<void(bool, const QString&, const QJsonDocument&)> callback,
                     int retryCount,
                     const QString &methodOverride,
                     bool suppressNetworkError,
                     AuthMode authMode);

    // 被登录守卫挂起的 Interactive 请求（登录成功后按入队顺序重放）。
    // 约束：request 是“发送前一刻”的对象——挂起时无 token，故不含 Authorization 头；
    // 补头与重放统一由 replayPendingAuthRequests 完成（见 .cpp），其他位置勿直接消费本结构。
    struct PendingAuthRequest {
        QNetworkAccessManager::Operation op;
        QNetworkRequest request;
        QByteArray body;
        QString methodOverride;
        std::function<void(int statusCode, const QByteArray &responseData)> handler;
    };
    // 登录成功后重放全部挂起请求（tokenChanged 触发；重放时 token 已非空，不会再次拦截）
    void replayPendingAuthRequests();
    // 清空挂起队列：每个请求按 -2（kAuthRequiredError）回调失败。两个触发点：
    // cancelAuthQueue（登录页被关闭/跳过）、setToken 置空（登出）。
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