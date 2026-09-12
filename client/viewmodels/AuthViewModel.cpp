#include "AuthViewModel.h"
#include <QPointer>
#include <gocook/IGoCookApi.h>
#include "../api/HttpGoCookApi.h"

AuthViewModel::AuthViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_db(LocalDatabase::instance())
    , m_loggedIn(false)
    , m_userId(0)
{
    m_api->setUnauthorizedHandler([self = QPointer<AuthViewModel>(this)]() {
        if (!self) return;
        if (self->m_loggedIn) { self->logout(); }
    });
}

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
            // IGoCookApi 边界防御：实现层（HttpGoCookApi）保证 error 非空；此处兜底其它实现的空文案
            emit self->loginFailed(QString::fromStdString(error.empty() ? "未知错误" : error));
            return;
        }
        if (data.token.empty() || data.user_id == 0) {
            emit self->loginFailed("登录失败：响应缺少令牌或用户ID");
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

void AuthViewModel::registerUser(const QString &username,
                                 const QString &password,
                                 const QString &email)
{
    gocook::models::RegisterRequest req;
    req.username = username.toStdString();
    req.password = password.toStdString();
    req.email    = email.toStdString();

    m_api->registerUser(req, [self = QPointer<AuthViewModel>(this)](bool success, const std::string &error) {
        if (!self) return;
        if (success) {
            // 两段式注册第一步完成：验证邮件已发送，等待输入验证码
            emit self->registerStarted();
        } else {
            // IGoCookApi 边界防御：实现层（HttpGoCookApi）保证 error 非空；此处兜底其它实现的空文案
            emit self->registerFailed(QString::fromStdString(
                error.empty() ? "未知错误" : error));
        }
    });
}

void AuthViewModel::verifyRegistration(const QString &email, const QString &token)
{
    m_api->verifyRegistration(email.toStdString(), token.toStdString(),
        [self = QPointer<AuthViewModel>(this)](bool success, const std::string &error) {
            if (!self) return;
            if (success) {
                emit self->registrationVerified();
            } else {
                // IGoCookApi 边界防御：实现层（HttpGoCookApi）保证 error 非空；此处兜底其它实现的空文案
                emit self->registrationVerifyFailed(QString::fromStdString(
                    error.empty() ? "未知错误" : error));
            }
        });
}

void AuthViewModel::logout()
{
    m_db->clearUser();
    m_api->setAuthToken("");
    setLoggedIn(false, 0, "");
    emit logoutFinished();
}

void AuthViewModel::checkAutoLogin()
{
    QVariantMap user = m_db->getUser();
    if (!user.isEmpty()) {
        QString token = user["token"].toString();
        m_api->setAuthToken(token.toStdString());
        m_api->getCurrentUser([self = QPointer<AuthViewModel>(this)](bool success, const gocook::models::UserProfile& profile, const std::string& error) {
            if (!self) return;
            if (success) {
                QVariantMap u = self->m_db->getUser();
                self->setLoggedIn(true, u["id"].toInt(), u["username"].toString());
                // 填充个人资料字段（头像 URL 等），否则重启后 ProfilePage 显示空白
                self->m_profileDisplayName = QString::fromStdString(profile.display_name);
                self->m_profileEmail = QString::fromStdString(profile.email);
                self->m_profilePhone = QString::fromStdString(profile.phone);
                self->m_profileAvatarUrl = QString::fromStdString(profile.avatar_url);
                emit self->profileChanged();
            } else {
                self->m_db->clearUser();
                self->m_api->setAuthToken("");
                if (self->m_loggedIn) {
                    self->setLoggedIn(false, 0, "");
                }
            }
            self->m_initialLoading = false;
            emit self->initialLoadingChanged();
        });
    } else {
        m_initialLoading = false;
        emit initialLoadingChanged();
    }
}

void AuthViewModel::loadProfile() {
    if (!m_loggedIn) return;
    m_api->getCurrentUser([self = QPointer<AuthViewModel>(this)](bool success,
                           const gocook::models::UserProfile& profile,
                           const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->profileSaveFailed(QString::fromStdString(error));
            return;
        }
        self->m_profileDisplayName = QString::fromStdString(profile.display_name);
        self->m_profileEmail = QString::fromStdString(profile.email);
        self->m_profilePhone = QString::fromStdString(profile.phone);
        self->m_profileAvatarUrl = QString::fromStdString(profile.avatar_url);
        emit self->profileChanged();
    });
}

void AuthViewModel::saveProfile(const QString &displayName,
                                 const QString &email,
                                 const QString &phone) {
    if (!m_loggedIn) return;

    gocook::models::UpdateProfileRequest req;
    if (!displayName.isEmpty())
        req.display_name = displayName.toStdString();
    if (!email.isEmpty())
        req.email = email.toStdString();
    if (!phone.isEmpty())
        req.phone = phone.toStdString();
    if (m_pendingAvatarId > 0)
        req.avatar_id = m_pendingAvatarId;

    m_api->updateProfile(req, [self = QPointer<AuthViewModel>(this)]
                         (bool success,
                          const gocook::models::UserProfile& profile,
                          const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->profileSaveFailed(QString::fromStdString(error));
            return;
        }
        self->m_profileDisplayName = QString::fromStdString(profile.display_name);
        self->m_profileEmail = QString::fromStdString(profile.email);
        self->m_profilePhone = QString::fromStdString(profile.phone);
        self->m_profileAvatarUrl = QString::fromStdString(profile.avatar_url);
        self->m_avatarVersion++;
        emit self->avatarVersionChanged();
        self->m_pendingAvatarId = 0;   // 头像已确认，清空待处理 ID
        emit self->profileChanged();
        emit self->profileSaved();
    });
}

void AuthViewModel::uploadAvatar(const QString &filePath) {
    if (!m_loggedIn) {
        emit avatarUploadFailed(QStringLiteral("未登录，请先登录"));
        return;
    }
    m_api->uploadAvatar(filePath.toStdString(), [self = QPointer<AuthViewModel>(this)]
                        (bool success,
                         const gocook::models::AvatarUploadResponse& resp,
                         const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->avatarUploadFailed(QString::fromStdString(error));
            return;
        }
        self->m_pendingAvatarId = resp.avatar_id;
        self->m_profileAvatarUrl = QString::fromStdString(resp.avatar_url);
        self->m_avatarVersion++;
        emit self->avatarVersionChanged();
        emit self->profileChanged();
        emit self->avatarUploaded(QString::fromStdString(resp.avatar_url));
    });
}

void AuthViewModel::changePassword(const QString &currentPassword, const QString &newPassword) {
    if (!m_loggedIn) {
        emit passwordChangeFailed(QStringLiteral("未登录，请先登录"));
        return;
    }
    m_api->changePassword(currentPassword.toStdString(),
                          newPassword.toStdString(),
                          [self = QPointer<AuthViewModel>(this)]
                          (bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            emit self->passwordChanged();
        } else {
            emit self->passwordChangeFailed(QString::fromStdString(
                error.empty() ? "修改失败" : error));
        }
    });
}

void AuthViewModel::deleteAccount() {
    if (!m_loggedIn) {
        emit accountDeleteFailed(QStringLiteral("未登录，请先登录"));
        return;
    }
    m_api->deleteAccount([self = QPointer<AuthViewModel>(this)]
                         (bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            // 清除本地状态并注销
            self->m_db->clearUser();
            self->m_api->setAuthToken("");
            self->setLoggedIn(false, 0, "");
            emit self->accountDeleted();
        } else {
            emit self->accountDeleteFailed(QString::fromStdString(
                error.empty() ? "注销失败" : error));
        }
    });
}

void AuthViewModel::loadPreferences() {
    if (!m_loggedIn) {
        emit preferencesLoadFailed(QStringLiteral("未登录，请先登录"));
        return;
    }
    m_api->getPreferences([self = QPointer<AuthViewModel>(this)]
                          (bool success,
                           const gocook::models::UserPreferences& prefs,
                           const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->preferencesLoadFailed(QString::fromStdString(
                error.empty() ? "获取偏好失败" : error));
            return;
        }
        QStringList likes, dislikes;
        for (const auto& v : prefs.likes)
            likes << QString::fromStdString(v);
        for (const auto& v : prefs.dislikes)
            dislikes << QString::fromStdString(v);
        QString healthGoal = QString::fromStdString(prefs.health_goal);
        emit self->preferencesLoaded(likes, dislikes, healthGoal);
    });
}

void AuthViewModel::savePreferences(const QStringList &likes,
                                     const QStringList &dislikes,
                                     const QString &healthGoal) {
    if (!m_loggedIn) {
        emit preferencesSaveFailed(QStringLiteral("未登录，请先登录"));
        return;
    }
    gocook::models::UserPreferences prefs;
    for (const auto& v : likes)
        prefs.likes.push_back(v.toStdString());
    for (const auto& v : dislikes)
        prefs.dislikes.push_back(v.toStdString());
    prefs.health_goal = healthGoal.toStdString();

    m_api->updatePreferences(prefs, [self = QPointer<AuthViewModel>(this)]
                             (bool success, const std::string& error) {
        if (!self) return;
        if (success) {
            emit self->preferencesSaved();
        } else {
            emit self->preferencesSaveFailed(QString::fromStdString(
                error.empty() ? "保存失败" : error));
        }
    });
}

void AuthViewModel::saveHealthProfile(int heightCm, double weightKg, const QStringList &conditions) {
    if (!m_loggedIn) {
        emit healthProfileSaveFailed(QStringLiteral("未登录，请先登录"));
        return;
    }
    gocook::models::HealthProfileRequest req;
    req.height_cm = heightCm;
    req.weight_kg = weightKg;
    for (const auto& v : conditions)
        req.conditions.push_back(v.toStdString());

    m_api->updateHealthProfile(req, [self = QPointer<AuthViewModel>(this)]
                               (bool success,
                                const gocook::models::HealthProfileResponse& resp,
                                const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->healthProfileSaveFailed(QString::fromStdString(
                error.empty() ? "保存失败" : error));
            return;
        }

        // 将忌口建议转换为 QVariantList 供 QML 使用
        QVariantList avoidances;
        for (const auto& item : resp.suggested_avoidances) {
            QVariantMap ai;
            ai["ingredient"] = QString::fromStdString(item.ingredient);
            ai["reason"] = QString::fromStdString(item.reason);
            avoidances.append(ai);
        }
        emit self->healthProfileSaved(avoidances);
    });
}

void AuthViewModel::loadHealthProfile() {
    if (!m_loggedIn) {
        emit healthProfileLoadFailed(QStringLiteral("未登录，请先登录"));
        return;
    }
    m_api->getHealthProfile([self = QPointer<AuthViewModel>(this)]
                            (bool success,
                             const gocook::models::HealthProfileResponse& resp,
                             const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->healthProfileLoadFailed(QString::fromStdString(
                error.empty() ? "获取健康指标失败" : error));
            return;
        }

        // 转换为 QML 友好的类型
        int height = resp.height_cm.has_value() ? resp.height_cm.value() : 0;
        double weight = resp.weight_kg.has_value() ? resp.weight_kg.value() : 0.0;
        QStringList conditions;
        for (const auto& c : resp.conditions)
            conditions << QString::fromStdString(c);
        QVariantList avoidances;
        for (const auto& item : resp.suggested_avoidances) {
            QVariantMap ai;
            ai["ingredient"] = QString::fromStdString(item.ingredient);
            ai["reason"] = QString::fromStdString(item.reason);
            avoidances.append(ai);
        }
        emit self->healthProfileLoaded(height, weight, conditions, avoidances);
    });
}

// ==================== 密码重置 ====================

void AuthViewModel::forgotPassword(const QString &username, const QString &email) {
    if (username.trimmed().isEmpty()) {
        emit forgotPasswordFailed(QStringLiteral("请输入用户名"));
        return;
    }
    if (email.trimmed().isEmpty()) {
        emit forgotPasswordFailed(QStringLiteral("请输入邮箱地址"));
        return;
    }
    m_api->forgotPassword(username.toStdString(), email.toStdString(),
        [self = QPointer<AuthViewModel>(this)](bool success, const std::string& error) {
            if (!self) return;
            if (success) {
                emit self->forgotPasswordSent();
            } else {
                emit self->forgotPasswordFailed(
                    QString::fromStdString(error.empty() ? "请求失败" : error));
            }
        });
}

void AuthViewModel::resetPassword(const QString &token, const QString &newPassword) {
    if (token.trimmed().isEmpty()) {
        emit passwordResetFailed(QStringLiteral("重置令牌无效"));
        return;
    }
    if (newPassword.size() < 6) {
        emit passwordResetFailed(QStringLiteral("密码不能少于6个字符"));
        return;
    }
    m_api->resetPassword(token.toStdString(), newPassword.toStdString(),
        [self = QPointer<AuthViewModel>(this)](bool success, const std::string& error) {
            if (!self) return;
            if (success) {
                emit self->passwordResetSuccess();
            } else {
                emit self->passwordResetFailed(
                    QString::fromStdString(error.empty() ? "重置失败" : error));
            }
        });
}

QString AuthViewModel::apiBaseUrl() const {
    // 返回 API 基础 URL，供 QML 拼接头像等静态资源 URL 使用
    // 实际从 HttpGoCookApi 获取，确保与 API 配置一致
    auto* httpApi = dynamic_cast<const HttpGoCookApi*>(m_api);
    if (httpApi)
        return httpApi->baseUrl();
    return QStringLiteral("http://127.0.0.1:8080");
}

void AuthViewModel::setLoggedIn(bool loggedIn, int userId, const QString &username)
{
    if (!loggedIn) {
        // 登出 / 注销 / token 失效：清空上一登录态的个人资料残留（头像等），避免游客态继续显示
        if (!m_profileDisplayName.isEmpty() || !m_profileEmail.isEmpty() ||
            !m_profilePhone.isEmpty() || !m_profileAvatarUrl.isEmpty()) {
            m_profileDisplayName.clear();
            m_profileEmail.clear();
            m_profilePhone.clear();
            m_profileAvatarUrl.clear();
            emit profileChanged();
        }
    }
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
