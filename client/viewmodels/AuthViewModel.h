#pragma once

#include <QObject>
#include <gocook/IGoCookApi.h>
#include "../database/LocalDatabase.h"

/**
 * @brief 认证域 ViewModel：登录 / 注册（两段式）/ 登出 / 自动登录 + 个人资料、偏好与健康档案。
 *
 * 所有 Q_INVOKABLE 异步（登录态与资料经 Q_PROPERTY + NOTIFY 驱动 QML）；凭证与用户行
 * 持久化到 LocalDatabase（生产用单例，测试注入）。
 *
 * 两条贯穿全类的设计（具体契约见各方法/信号注释）：
 *   1. 启动自动登录（main.cpp 启动时调用 checkAutoLogin）：网络故障不清凭证——保留 token
 *      乐观进入主界面（断网仍可读快照）；仅当 401 明确拒绝令牌才清理为游客；
 *      联网后首个 401 由 unauthorizedHandler 自动登出。
 *   2. 登录态守卫：需登录的方法大多未登录时直接发对应 *Failed（“未登录，请先登录”）；
 *      loadProfile / saveProfile 为静默 return——见各方法注释。
 */
class AuthViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)
    Q_PROPERTY(int userId READ userId NOTIFY userIdChanged)
    Q_PROPERTY(bool initialLoading READ initialLoading NOTIFY initialLoadingChanged)   ///< 启动自动登录校验中（checkAutoLogin 完成前为 true）

    // 个人资料属性
    Q_PROPERTY(QString profileDisplayName READ profileDisplayName NOTIFY profileChanged)
    Q_PROPERTY(QString profileEmail READ profileEmail NOTIFY profileChanged)
    Q_PROPERTY(QString profilePhone READ profilePhone NOTIFY profileChanged)
    Q_PROPERTY(QString profileAvatarUrl READ profileAvatarUrl NOTIFY profileChanged)
    Q_PROPERTY(int avatarVersion READ avatarVersion NOTIFY avatarVersionChanged)   ///< 头像缓存版本号（保存成功后递增，QML 拼 URL 防缓存）

    // API 基础 URL（用于 QML 拼接头像等静态资源 URL）
    Q_PROPERTY(QString apiBaseUrl READ apiBaseUrl CONSTANT)

public:
    /// db：本地库（凭证/快照）。生产传 nullptr（单例 LocalDatabase::instance()）；测试注入独立内存库
    explicit AuthViewModel(IGoCookApi *api, QObject *parent = nullptr, LocalDatabase *db = nullptr);

    // getter 组：返回同名 Q_PROPERTY 的值（语义见属性行）；仅特殊实现单独注明。
    bool loggedIn() const { return m_loggedIn; }
    QString username() const { return m_username; }
    int userId() const { return m_userId; }
    bool initialLoading() const { return m_initialLoading; }

    QString profileDisplayName() const { return m_profileDisplayName; }
    QString profileEmail() const { return m_profileEmail; }
    QString profilePhone() const { return m_profilePhone; }
    QString profileAvatarUrl() const { return m_profileAvatarUrl; }
    int avatarVersion() const { return m_avatarVersion; }
    QString apiBaseUrl() const;   ///< 特殊实现：dynamic_cast 到 HttpGoCookApi 取服务端地址；非 HTTP 实现回退 http://127.0.0.1:8080

    /// 登录：成功写凭证到本地库并生效 token，发 loginSuccess；失败发 loginFailed。
    Q_INVOKABLE void login(const QString &username, const QString &password);
    /// 注册第一步：发验证邮件（成功发 registerStarted，随后走 verifyRegistration）。
    Q_INVOKABLE void registerUser(const QString &username, const QString &password, const QString &email);
    /// 注册第二步：验证码核验。成功发 registrationVerified，失败发 registrationVerifyFailed。
    Q_INVOKABLE void verifyRegistration(const QString &email, const QString &token);
    /// 登出：纯本地清理（清凭证与资料），发 logoutFinished；不发网络请求。
    Q_INVOKABLE void logout();
    /// 自动登录校验（启动时调用）；完成（无论成败）后 initialLoading=false，分支见类头。
    Q_INVOKABLE void checkAutoLogin();

    // 个人资料管理
    /// 拉取个人资料（未登录静默忽略）。注意：失败发 profileSaveFailed（无独立加载失败信号）。
    Q_INVOKABLE void loadProfile();
    /// 保存资料（未登录静默忽略；携带上次上传的暂存头像，保存成功才绑定生效）。
    /// 成功发 profileSaved；失败发 profileSaveFailed。
    Q_INVOKABLE void saveProfile(const QString &displayName, const QString &email, const QString &phone);
    /// 上传头像（仅暂存，不改变已保存资料）：成功发 avatarUploaded(暂存 url)；
    /// 未登录/失败发 avatarUploadFailed。暂存须经 saveProfile 绑定才生效；
    /// 重传时旧暂存保留至新上传成功（失败不丢旧暂存，预览与"保存将绑定"语义一致）。
    Q_INVOKABLE void uploadAvatar(const QString &filePath);
    /// 是否有未保存的暂存头像（编辑页"返回"回滚判断用）。
    Q_INVOKABLE bool hasPendingAvatar() const;
    /// 放弃未保存的暂存头像（编辑页"返回"时调用，幂等）：本地立即清空（UI 先回滚），
    /// 服务端删除尽力而为（失败由服务端 GC 兜底）；同时作废在途上传（其文件由回调侧丢弃）。
    Q_INVOKABLE void discardPendingAvatar();
    /// 修改密码。成功发 passwordChanged；未登录/失败发 passwordChangeFailed。
    Q_INVOKABLE void changePassword(const QString &currentPassword, const QString &newPassword);
    /// 注销账号：成功后本地清理并登出（发 accountDeleted，不发 logoutFinished）；失败发 accountDeleteFailed。
    Q_INVOKABLE void deleteAccount();
    /// 拉取偏好。成功发 preferencesLoaded(likes, dislikes, healthGoal)；未登录/失败发 preferencesLoadFailed。
    Q_INVOKABLE void loadPreferences();
    /// 保存偏好。成功发 preferencesSaved；未登录/失败发 preferencesSaveFailed。
    Q_INVOKABLE void savePreferences(const QStringList &likes, const QStringList &dislikes, const QString &healthGoal);
    /// 保存健康档案。成功发 healthProfileSaved（avoidances: [{ingredient, reason}]）；失败发 healthProfileSaveFailed。
    Q_INVOKABLE void saveHealthProfile(int heightCm, double weightKg, const QStringList &conditions);
    /// 拉取健康档案。成功发 healthProfileLoaded（height/weight 缺省为 0）；失败发 healthProfileLoadFailed。
    Q_INVOKABLE void loadHealthProfile();

    // 密码重置
    /// 申请密码重置邮件（用户名/邮箱空白时直接发 forgotPasswordFailed）。成功发 forgotPasswordSent。
    Q_INVOKABLE void forgotPassword(const QString &username, const QString &email);
    /// 重置密码（token 空或新密码不足 6 位时直接发 passwordResetFailed）。成功发 passwordResetSuccess。
    Q_INVOKABLE void resetPassword(const QString &token, const QString &newPassword);

signals:
    // 注：*Changed 为对应 Q_PROPERTY 的 NOTIFY 伴侣；结果信号的触发与 payload 见对应方法注释。
    void loggedInChanged();
    void usernameChanged();
    void userIdChanged();
    void loginSuccess();
    void loginFailed(const QString &error);
    void registerStarted();   ///< 注册第一步成功：验证邮件已发送（等待 verifyRegistration）
    void registerFailed(const QString &error);
    void registrationVerified();   ///< verifyRegistration 成功
    void registrationVerifyFailed(const QString &error);   ///< verifyRegistration 失败
    void logoutFinished();
    void initialLoadingChanged();

    // 个人资料信号
    void profileChanged();
    void avatarVersionChanged();
    void profileSaved();
    void profileSaveFailed(const QString &error);
    void avatarUploaded(const QString &avatarUrl);   ///< 头像已上传为暂存（保存后才生效）
    void avatarUploadFailed(const QString &error);
    void avatarDiscarded(const QString &avatarUrl);  ///< 放弃暂存头像尝试完成（无论服务端删除成败）
    void passwordChanged();
    void passwordChangeFailed(const QString &error);
    void accountDeleted();
    void accountDeleteFailed(const QString &error);
    void preferencesLoaded(const QStringList &likes, const QStringList &dislikes, const QString &healthGoal);
    void preferencesLoadFailed(const QString &error);
    void preferencesSaved();
    void preferencesSaveFailed(const QString &error);
    void healthProfileSaved(const QVariantList &avoidances);
    void healthProfileSaveFailed(const QString &error);
    void healthProfileLoaded(int heightCm, double weightKg, const QStringList &conditions, const QVariantList &avoidances);
    void healthProfileLoadFailed(const QString &error);

    // 密码重置信号
    void forgotPasswordSent();
    void forgotPasswordFailed(const QString &error);
    void passwordResetSuccess();
    void passwordResetFailed(const QString &error);

private:
    void setLoggedIn(bool loggedIn, int userId = 0, const QString &username = "");   ///< 统一写入登录身份三属性（变化时发信号）；登出时清空资料残留

    IGoCookApi *m_api;   ///< API 门面（构造注入）
    LocalDatabase *m_db;   ///< 本地库（生产=单例；测试注入独立库）
    bool m_loggedIn;   ///< 登录态（true=token 已生效并落库）
    int m_userId;   ///< 当前用户 id（未登录=0）
    QString m_username;   ///< 当前用户名
    bool m_initialLoading = true;   ///< 启动自动登录校验中（checkAutoLogin 完成置 false）
    /// 自动登录校验窗口：checkAutoLogin 发起 /api/users/me 校验期间为 true；
    /// 窗口内收到 401（由 unauthorizedHandler 回调标记）即“令牌被服务端明确拒绝”——
    /// checkAutoLogin 失败分支据此区分“会话失效（清理）”与“瞬时故障（保留凭证+快照）”
    bool m_autoLoginInFlight = false;
    bool m_autoLoginUnauthorized = false;

    // 个人资料数据
    QString m_profileDisplayName;   ///< 显示名
    QString m_profileEmail;   ///< 邮箱
    QString m_profilePhone;   ///< 手机号
    QString m_profileAvatarUrl;   ///< 已保存生效的头像 URL（暂存预览不写这里）
    QString m_pendingAvatarUrl;   ///< 上次上传待保存的暂存头像 URL；saveProfile 时随请求绑定，放弃时清空
    int m_avatarUploadSeq = 0;    ///< 上传代次：放弃/重传时作废旧回调，防止陈旧上传覆盖新状态
    int m_avatarVersion = 0;      ///< 头像缓存版本号，每次保存成功后递增
};
