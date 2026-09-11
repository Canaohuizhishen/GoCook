#include "UserServiceImpl.h"
#include <jwt-cpp/jwt.h>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>
#include <cctype>
#include "../common/Logger.h"
#include "../common/EmailSender.h"
#include "bcrypt/crypt_blowfish.h"
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include "HealthConditionLists.h"

#define BCRYPT_OUTPUT_SIZE 128

using namespace gocook::models;
using namespace gocook::services;

namespace {
    // 把名单食材 join 成 UI 文案（“、”分隔，顺序 = 名单顺序）
    std::string joinAvoidanceNames(const std::vector<std::string>& names) {
        std::string s;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i) s += "、";
            s += names[i];
        }
        return s;
    }

    void populateAvoidanceSuggestions(
        const std::vector<std::string>& conditions,
        std::vector<AvoidanceItem>& out)
    {
        for (const auto& c : conditions) {
            // 食材名单一律取自 HealthConditionLists.h（唯一事实源，杜绝手抄漂移）；
            // reason 文案属 UI 文案层，本地维护。
            const auto& hard = gocook::health::hardExcludedFor(c);
            const auto& soft = gocook::health::softAdvisedFor(c);
            if (c == "高血压") {
                if (!hard.empty())
                    out.push_back({joinAvoidanceNames(hard), "高盐加工品，推荐结果将直接排除含此类食材的菜谱"});
                if (!soft.empty())
                    out.push_back({joinAvoidanceNames(soft), "调味高钠，系统仅提示少放，不屏蔽菜谱"});
            } else if (c == "高血脂") {
                if (!hard.empty())
                    out.push_back({joinAvoidanceNames(hard), "高脂食材，推荐结果将直接排除含此类食材的菜谱"});
            } else if (c == "糖尿病") {
                if (!hard.empty())
                    out.push_back({joinAvoidanceNames(hard), "高糖成品酱料，推荐结果将直接排除含此类食材的菜谱"});
                if (!soft.empty())
                    out.push_back({joinAvoidanceNames(soft), "调味糖源可少放或不放，系统仅提示不屏蔽"});
            } else if (c == "胃炎") {
                // 引擎不对胃炎做食材级过滤（辛辣/生冷无法映射食材黑名单）——纯建议口径，不声称排除
                out.push_back({"辛辣食物", "刺激胃黏膜，可能加重炎症（系统仅作建议，不做菜谱排除）"});
                out.push_back({"生冷食物", "不易消化，增加胃负担（系统仅作建议，不做菜谱排除）"});
                out.push_back({"酒精", "刺激胃黏膜，应避免饮酒（系统仅作建议，不做菜谱排除）"});
            } else if (c == "痛风") {
                if (!hard.empty())
                    out.push_back({joinAvoidanceNames(hard), "高嘌呤/高风险食材，推荐结果将直接排除含此类食材的菜谱"});
            }
        }
    }

    /// 生成 6 位数字验证码（CSPRNG，范围 100000-999999；密码重置与注册验证共用）
    std::string generateNumericCode() {
        unsigned char randomBytes[4];
        if (RAND_bytes(randomBytes, sizeof(randomBytes)) != 1) {
            throw ServiceException("无法生成安全令牌");
        }
        uint32_t val = (static_cast<uint32_t>(randomBytes[0]) << 24)
                     | (static_cast<uint32_t>(randomBytes[1]) << 16)
                     | (static_cast<uint32_t>(randomBytes[2]) << 8)
                     | static_cast<uint32_t>(randomBytes[3]);
        return std::to_string((val % 900000) + 100000);
    }

    /// 发送邮件：SMTP 已配置 → 真发（失败抛 500，注册的两个分支一致，响应层无可区分差异）；
    /// 未配置（开发模式）→ 邮件内容打印到服务端日志，接口响应保持统一。
    void sendMailOrLog(const std::string& to, const std::string& subject, const std::string& body) {
        if (EmailSender::isConfigured()) {
            if (!EmailSender::sendEmail(to, subject, body)) {
                LOG_ERROR("向 %s 发送邮件失败（SMTP 错误）：%s", to.c_str(), subject.c_str());
                throw ServiceException("邮件发送失败，请稍后再试或联系管理员", 500);
            }
            LOG_INFO("邮件已发送至 %s：%s", to.c_str(), subject.c_str());
        } else {
            LOG_WARN("[DEV MAIL] SMTP 未配置，邮件未真实发送\n收件人：%s\n主题：%s\n正文：\n%s",
                     to.c_str(), subject.c_str(), body.c_str());
        }
    }
}

std::string UserServiceImpl::generateToken(int userId, const std::string& username, const std::string& role) {
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::hours(24 * 7);

    auto token = jwt::create()
                     .set_issuer("GoCook")
                     .set_type("JWS")
                     .set_payload_claim("userId", jwt::claim(std::to_string(userId)))
                     .set_payload_claim("username", jwt::claim(username))
                     .set_payload_claim("role", jwt::claim(role))
                     .set_issued_at(now)
                     .set_expires_at(exp)
                     .sign(jwt::algorithm::hs256{jwt_secret_});

    return token;
}

std::string UserServiceImpl::hashPassword(const std::string& plain) {
    // 1. 生成 16 字节（128位）的随机盐
    unsigned char random_bytes[16];
    if (RAND_bytes(random_bytes, sizeof(random_bytes)) != 1) {
        throw ServiceException("无法生成安全随机盐");
    }

    // 2. 把随机字节“格式化”成 bcrypt 能识别的盐字符串
    char salt[BCRYPT_OUTPUT_SIZE];
    char *salt_result = _crypt_gensalt_blowfish_rn("$2a$", 10, reinterpret_cast<const char*>(random_bytes), 16, salt, sizeof(salt));
    if (!salt_result) {
        throw ServiceException("无法生成密码盐值");
    }
    // 此时 salt_result 内容大概长这样： "$2a$10$abcdefghijklmnopqrstuv"（22位随机字符）
    // 其中 "$2a$" 是算法标识， "10" 是计算成本（2^10轮哈希）

    // 3. 核心哈希动作：用上面生成的盐，对明文密码做 blowfish 加密（单向散列）
    char hash[BCRYPT_OUTPUT_SIZE];
    char *hash_result = _crypt_blowfish_rn(plain.c_str(), salt, hash, sizeof(hash));
    if (!hash_result) {
        throw ServiceException("密码哈希计算失败");
    }
    // 此时 hash_result 内容大概长这样：
    // "$2a$10$abcdefghijklmnopqrstuvxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"（60位完整字符串）

    // 注意：这个最终结果里，包含了算法、成本、盐、和最终的哈希值，四合一。
    return std::string(hash_result);
}

bool UserServiceImpl::validatePassword(const std::string& plain, const std::string& hash) {
    char output[BCRYPT_OUTPUT_SIZE];
    // 函数内部会（直接截取 hash 的前22位字符）自动解析 hash 里的 "$2a$"、"10"、"盐"，然后用它们去加密 plain，得到的结果用于后续的比较
    char *result = _crypt_blowfish_rn(plain.c_str(), hash.c_str(), output, sizeof(output));
    if (!result) {
        return false;
    }

    // 先比较长度（防止长度不一致导致下面的 memcmp 越界）
    size_t result_len = std::strlen(result);
    if (hash.size() != result_len) {
        return false;
    }

    // 常量时间比较（防计时攻击）
    return CRYPTO_memcmp(hash.data(), result, result_len) == 0;
}

void UserServiceImpl::registerUser(const RegisterRequest& request) {
    // 用户名冲突照常返回 409（用户名维度不在防枚举收口范围）
    auto existing = userRepo_->findByUsername(request.username);
    if (existing.has_value()) {
        throw ServiceException("用户名已存在", 409);
    }

    // 邮箱已注册：接口响应保持统一（不泄露注册状态），差异只体现在邮件内容上（防枚举）；
    // 邮件发送失败在两种分支都会抛 500，响应层不产生可区分的差异。
    if (userRepo_->existsByEmail(request.email)) {
        sendMailOrLog(request.email, "GoCook 注册提示",
            "此邮箱已关联 GoCook 账户。请直接登录；如果忘记密码，可在登录页尝试找回密码。\n"
            "如非本人操作，请忽略此邮件。");
        return;
    }

    // 两段式注册第一步：写入待验证记录（覆盖旧记录）并发送验证码邮件，不直接建号
    std::string token = generateNumericCode();
    std::string hashed = hashPassword(request.password);
    userRepo_->upsertPendingRegistration(request.username, hashed, request.email, token);
    sendMailOrLog(request.email, "GoCook 注册验证码",
        "您正在注册 GoCook 账户，验证码为：" + token + "（15 分钟内有效）。\n"
        "请在注册页面输入该验证码完成注册。如非本人操作，请忽略此邮件。");
}

void UserServiceImpl::verifyRegistration(const std::string& email, const std::string& token) {
    // 两段式注册第二步：验证码核验通过后原子建号；此阶段可返回具体错误（持码人已证明邮箱归属）
    switch (userRepo_->createUserFromPendingRegistration(email, token)) {
        case RegistrationOutcome::Success:
            return;
        case RegistrationOutcome::InvalidCode:
            throw ServiceException("验证码无效或已过期", 400);
        case RegistrationOutcome::UsernameTaken:
            throw ServiceException("用户名已被占用，请更换用户名后重新注册", 409);
        case RegistrationOutcome::EmailTaken:
            throw ServiceException("该邮箱已被注册，请直接登录", 409);
    }
    throw ServiceException("注册验证失败");
}

LoginResponse UserServiceImpl::login(const LoginRequest& request) {
    auto authInfo = userRepo_->findByUsername(request.username);
    if (!authInfo.has_value()) {
        throw ServiceException("用户名或密码错误", 401);
    }

    if (!validatePassword(request.password, authInfo->passwordHash)) {
        throw ServiceException("用户名或密码错误", 401);
    }

    LoginResponse resp;
    resp.user_id = authInfo->id;
    resp.username = authInfo->username;
    resp.token = generateToken(authInfo->id, authInfo->username, authInfo->role);
    return resp;
}

UserProfile UserServiceImpl::getCurrentUser(int userId) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) {
        throw ServiceException("用户不存在", 404);
    }
    return *user;
}

// ==================== 密码重置 ====================

std::optional<std::string> UserServiceImpl::requestPasswordReset(const std::string& username,
                                                                   const std::string& email) {
    // 1. 校验用户名与邮箱是否匹配（双重验证）
    auto userIdOpt = userRepo_->findIdByUsernameAndEmail(username, email);
    if (!userIdOpt.has_value()) {
        LOG_WARN("密码重置请求未找到匹配账户：username='%s' email='%s'",
                 username.c_str(), email.c_str());
        throw ServiceException("用户名或邮箱不正确", 400);
    }

    // 开发模式下返回 dev_token 仅发生在「用户名+邮箱双双匹配」时（配 SMTP 的生产模式不返回令牌）；
    // 双字段匹配即发信门槛——不知道完整用户名+邮箱组合的请求不会触发任何邮件（防轰炸）。
    // 2. 生成 6 位数字验证码（CSPRNG，与注册验证共用 generateNumericCode）
    std::string token = generateNumericCode();

    // 3. 将令牌存入数据库 — 过期时间在 SQL 中计算为 NOW() + INTERVAL '15 minutes'
    userRepo_->createPasswordResetToken(userIdOpt.value(), token);

    // 5. 发送邮件
    //    - SMTP 已配置且发送成功 → 返回 nullopt（令牌已通过邮件发送）
    //    - SMTP 未配置 → 返回令牌用于开发模式响应
    //      （比打印到 stderr 更安全 — 令牌直接返回给 API 调用方，而非写入日志）
    //    - SMTP 已配置但发送失败 → 抛出异常（真实错误）
    if (EmailSender::isConfigured()) {
        bool sent = EmailSender::sendPasswordResetEmail(email, token);
        if (!sent) {
            LOG_ERROR("向 %s 发送邮件失败（SMTP 错误）", email.c_str());
            throw ServiceException("密码重置邮件发送失败，请稍后再试或联系管理员", 500);
        }
        LOG_INFO("密码重置邮件已发送至 %s", email.c_str());
        return std::nullopt;
    } else {
        LOG_WARN("SMTP 未配置——密码重置令牌将随响应返回（开发模式）");
        return token;
    }
}

void UserServiceImpl::resetPassword(const std::string& token, const std::string& newPassword) {
    // \note 已知限制：密码重置后，旧的 JWT 令牌在过期前仍然有效。
    //       当前无服务器端令牌黑名单/版本号机制。
    // 1. 校验令牌 — 从有效（未使用且未过期）的令牌中查找 user_id
    auto userIdOpt = userRepo_->findUserIdByResetToken(token);
    if (!userIdOpt.has_value()) {
        throw ServiceException("令牌无效或已过期", 400);
    }

    // 2. 校验新密码强度（复用 changePassword 的相同规则）
    if (newPassword.size() < 6) {
        throw ServiceException("密码需包含字母和数字，至少6位", 400);
    }
    bool hasLetter = false, hasDigit = false;
    for (char c : newPassword) {
        if (std::isalpha(static_cast<unsigned char>(c))) hasLetter = true;
        if (std::isdigit(static_cast<unsigned char>(c))) hasDigit = true;
    }
    if (!hasLetter || !hasDigit) {
        throw ServiceException("密码需包含字母和数字，至少6位", 400);
    }

    // 3. 对新密码进行哈希
    std::string newHash = hashPassword(newPassword);

    // 4. 在单个事务中更新密码并将令牌标记为已使用（防重放）
    userRepo_->resetPasswordAndMarkTokenUsed(userIdOpt.value(), newHash, token);
}
UserProfile UserServiceImpl::updateProfile(int userId, const UpdateProfileRequest& profile) {
    // 校验：至少需提供一个字段
    if (!profile.display_name.has_value() && !profile.avatar_url.has_value() &&
        !profile.avatar_id.has_value() && !profile.email.has_value() &&
        !profile.phone.has_value()) {
        throw ServiceException("没有提供需要更新的字段", 400);
    }

    // 校验 display_name 不为空
    if (profile.display_name.has_value() && profile.display_name->empty()) {
        throw ServiceException("昵称不能为空", 400);
    }

    // 解析 avatar_id：若 avatar_id 等于 userId，保留当前 avatar_url
    // （上传时已设置）。若 avatar_id 已设置但不同，则忽略它。
    UpdateProfileRequest resolvedProfile = profile;
    if (profile.avatar_id.has_value() && profile.avatar_id.value() == userId) {
        // 保留现有 avatar_url — 将 resolvedProfile.avatar_url 设为 nullopt
        // 以免仓库层的 COALESCE 覆盖它
        resolvedProfile.avatar_url = std::nullopt;
    }

    // 若邮箱被修改则检查唯一性
    if (profile.email.has_value() && !profile.email->empty()) {
        auto currentUser = userRepo_->findById(userId);
        if (currentUser.has_value() && profile.email.value() != currentUser->email) {
            if (userRepo_->existsByEmail(profile.email.value())) {
                throw ServiceException("邮箱已被注册", 409);
            }
        }
    }

    userRepo_->updateProfile(userId, resolvedProfile);

    // 返回更新后的用户资料
    auto updated = userRepo_->findById(userId);
    if (!updated.has_value()) {
        throw ServiceException("用户不存在", 404);
    }
    return *updated;
}
/// \note 已知限制：密码修改后，旧的 JWT 令牌在过期前仍然有效。
///       当前无服务器端令牌黑名单/版本号机制。
///       若需立即吊销令牌，后续需引入 token 版本号字段并嵌入 JWT payload。
void UserServiceImpl::changePassword(int userId,
                                      const std::string& currentPassword,
                                      const std::string& newPassword) {
    // 1. 获取当前密码哈希
    std::string currentHash = userRepo_->getPasswordHash(userId);

    // 2. 验证原密码
    if (!validatePassword(currentPassword, currentHash)) {
        throw ServiceException("原密码不正确", 400);
    }

    // 3. 验证新密码强度（至少6位，含字母和数字）
    if (newPassword.size() < 6) {
        throw ServiceException("密码需包含字母和数字，至少6位", 400);
    }
    bool hasLetter = false, hasDigit = false;
    for (char c : newPassword) {
        if (std::isalpha(static_cast<unsigned char>(c))) hasLetter = true;
        if (std::isdigit(static_cast<unsigned char>(c))) hasDigit = true;
    }
    if (!hasLetter || !hasDigit) {
        throw ServiceException("密码需包含字母和数字，至少6位", 400);
    }

    // 4. 对新密码进行哈希
    std::string newHash = hashPassword(newPassword);

    // 5. 更新数据库
    userRepo_->changePassword(userId, newHash);
}
void UserServiceImpl::deleteAccount(int userId) {
    // 先确认用户存在
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) {
        throw ServiceException("用户不存在", 404);
    }
    userRepo_->deleteAccount(userId);
}
AvatarUploadResponse UserServiceImpl::uploadAvatar(int userId, const std::string& filePath) {
    if (filePath.empty()) {
        throw ServiceException("文件路径无效", 400);
    }
    return userRepo_->uploadAvatar(userId, filePath);
}
UserPreferences UserServiceImpl::getPreferences(int userId) {
    // 先确认用户存在
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) {
        throw ServiceException("用户不存在", 404);
    }
    return userRepo_->getPreferences(userId);
}
void UserServiceImpl::updatePreferences(int userId, const UserPreferences& prefs) {
    // 先确认用户存在
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) {
        throw ServiceException("用户不存在", 404);
    }
    userRepo_->updatePreferences(userId, prefs);
}
HealthProfileResponse UserServiceImpl::updateHealthProfile(int userId, const HealthProfileRequest& req) {
    // 先确认用户存在
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) {
        throw ServiceException("用户不存在", 404);
    }

    // 将健康档案保存到数据库
    userRepo_->updateHealthProfile(userId, req);

    // 根据用户的健康情况生成忌口建议
    HealthProfileResponse resp;
    // 填充已保存的字段，以备调用方需要
    resp.height_cm = req.height_cm;
    resp.weight_kg = req.weight_kg;
    resp.conditions = req.conditions;
    // 填充忌口建议
    populateAvoidanceSuggestions(req.conditions, resp.suggested_avoidances);
    return resp;
}

HealthProfileResponse UserServiceImpl::getHealthProfile(int userId) {
    // 先确认用户存在
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) {
        throw ServiceException("用户不存在", 404);
    }

    // 从数据库加载健康档案
    auto resp = userRepo_->getHealthProfile(userId);

    // 根据已保存的健康情况生成忌口建议
    populateAvoidanceSuggestions(resp.conditions, resp.suggested_avoidances);

    return resp;
}

PagedFavorites UserServiceImpl::getFavorites(int userId, int page, int size, const std::string& group) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    return userRepo_->getFavorites(userId, page, size, group);
}
std::vector<FavoriteGroup> UserServiceImpl::getFavoriteGroups(int userId) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    return userRepo_->getFavoriteGroups(userId);
}
FavoriteGroup UserServiceImpl::createFavoriteGroup(int userId, const CreateGroupRequest& req) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    return userRepo_->createFavoriteGroup(userId, req);
}
void UserServiceImpl::updateFavoriteGroup(int userId, int groupId, const UpdateGroupRequest& req) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    userRepo_->updateFavoriteGroup(userId, groupId, req);
}
void UserServiceImpl::deleteFavoriteGroup(int userId, int groupId) {
    userRepo_->deleteFavoriteGroup(userId, groupId);
}
void UserServiceImpl::updateFavoriteItem(int userId, int favoriteId, const UpdateFavoriteRequest& req) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    userRepo_->updateFavoriteItem(userId, favoriteId, req);
}
void UserServiceImpl::batchDeleteFavorites(int userId, const BatchDeleteFavoritesRequest& req) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    userRepo_->batchDeleteFavorites(userId, req);
}
PagedNotifications UserServiceImpl::getNotifications(int userId, int page, int size, const std::string& type) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    return userRepo_->getNotifications(userId, page, size, type);
}
void UserServiceImpl::markNotificationRead(int userId, int notificationId) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    userRepo_->markNotificationRead(userId, notificationId);
}
void UserServiceImpl::markAllNotificationsRead(int userId) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    userRepo_->markAllNotificationsRead(userId);
}
void UserServiceImpl::deleteNotification(int userId, int notificationId) {
    auto user = userRepo_->findById(userId);
    if (!user.has_value()) throw ServiceException("用户不存在", 404);
    userRepo_->deleteNotification(userId, notificationId);
}
