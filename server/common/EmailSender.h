#pragma once

#include <string>
#include <optional>

/**
 * @brief 轻量 SMTP 邮件发送器
 *
 * 使用 OpenSSL 建立 TLS 连接，通过 SMTP 协议发送邮件。
 * 配置通过环境变量读取：
 *   GOCOOK_SMTP_HOST     — SMTP 服务器地址（默认 smtp.gmail.com）
 *   GOCOOK_SMTP_PORT     — SMTP 端口（默认 465）
 *   GOCOOK_SMTP_USER     — SMTP 用户名（邮箱地址）
 *   GOCOOK_SMTP_PASS     — SMTP 密码/应用专用密码
 *   GOCOOK_SMTP_FROM     — 发件人地址（默认同 GOCOOK_SMTP_USER）
 */
class EmailSender {
public:
    /// 发送密码重置邮件
    static bool sendPasswordResetEmail(const std::string& toEmail,
                                       const std::string& token);
    /// 发送通用邮件
    static bool sendEmail(const std::string& to,
                          const std::string& subject,
                          const std::string& body);

private:
    /// SMTP 配置
    struct SmtpConfig {
        std::string host = "smtp.gmail.com";
        int port = 587;
        std::string username;
        std::string password;
        std::string from;
        bool valid = false;
    };

    static SmtpConfig loadConfig();
    static bool sendRaw(SmtpConfig& cfg, const std::string& to,
                        const std::string& subject, const std::string& body);
};
