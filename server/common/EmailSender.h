#pragma once

#include <string>
#include <optional>

/**
 * @brief 轻量 SMTP 邮件发送器
 *
 * 使用 OpenSSL 建立 TLS 连接，通过 SMTP 协议发送邮件。
 *
 * 缺陷：只覆盖了最简会话流程。
 * 它没有实现重连队列、没有处理超时重传、没有做 DKIM/SPF 签名（如果 GOCOOK_SMTP_FROM 和实际中继服务器不一致，邮件必进垃圾箱）。
 * 所以在当前形态下，它只能配合 Gmail/163 等公共中继使用，不能作为生产级企业邮件网关。
 *
 * 配置通过环境变量读取：
 *   GOCOOK_SMTP_HOST     — SMTP 服务器地址（默认 smtp.gmail.com）
 *   GOCOOK_SMTP_PORT         — SMTP 端口（默认 465；587 自动使用 STARTTLS）
 *   GOCOOK_SMTP_USER         — SMTP 用户名（邮箱地址）
 *   GOCOOK_SMTP_PASS         — SMTP 密码/应用专用密码
 *   GOCOOK_SMTP_FROM         — 发件人地址（默认同 GOCOOK_SMTP_USER）
 *   GOCOOK_SMTP_TLS_INSECURE — 设为 true 可跳过证书验证（仅开发环境）
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

    /// SMTP 是否已配置（有用户名和密码）
    static bool isConfigured();

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
