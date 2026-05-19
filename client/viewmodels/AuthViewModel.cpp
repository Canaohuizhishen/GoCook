#include "AuthViewModel.h"
#include <QPointer>
#include <gocook/IGoCookApi.h>

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
            emit self->loginFailed(QString::fromStdString(error.empty() ? "Unknown error" : error));
            return;
        }
        if (data.token.empty() || data.user_id == 0) {
            emit self->loginFailed("Login failed: missing token or user_id");
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
            emit self->registerSuccess();
        } else {
            emit self->registerFailed(QString::fromStdString(
                error.empty() ? "Unknown error" : error));
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
            Q_UNUSED(profile)
            if (!self) return;
            if (success) {
                QVariantMap u = self->m_db->getUser();
                self->setLoggedIn(true, u["id"].toInt(), u["username"].toString());
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
        emit self->profileChanged();
        emit self->profileSaved();
    });
}

void AuthViewModel::uploadAvatar(const QString &filePath) {
    if (!m_loggedIn) return;
    m_api->uploadAvatar(filePath.toStdString(), [self = QPointer<AuthViewModel>(this)]
                        (bool success,
                         const gocook::models::AvatarUploadResponse& resp,
                         const std::string& error) {
        if (!self) return;
        if (!success) {
            emit self->avatarUploadFailed(QString::fromStdString(error));
            return;
        }
        self->m_profileAvatarUrl = QString::fromStdString(resp.avatar_url);
        emit self->profileChanged();
        emit self->avatarUploaded(QString::fromStdString(resp.avatar_url));
    });
}

void AuthViewModel::setLoggedIn(bool loggedIn, int userId, const QString &username)
{
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
