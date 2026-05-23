#include "EmailSender.h"
#include "Logger.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace {

// 从 BIO 读取一行（直到 \n），返回去掉末尾 \r\n 的字符串
std::string readLine(BIO* bio) {
    std::string line;
    char ch;
    while (BIO_read(bio, &ch, 1) > 0) {
        if (ch == '\n') break;
        if (ch != '\r') line += ch;
    }
    return line;
}

// 发送 SMTP 命令并读取响应码（自动排干多行响应）
int sendCommand(BIO* bio, const std::string& cmd, std::string& response,
                bool drainMultiline = false) {
    if (cmd.size() > 0) {
        std::string fullCmd = cmd + "\r\n";
        BIO_write(bio, fullCmd.data(), fullCmd.size());
        BIO_flush(bio);
    }
    response = readLine(bio);
    if (response.empty()) return -1;
    int code = std::stoi(response.substr(0, 3));
    // 如果响应行是 NNN- 开头（多行响应），继续读取直到 NNN 空格 开头
    if (drainMultiline && response.size() >= 4 && response[3] == '-') {
        std::string lastLine;
        do {
            lastLine = readLine(bio);
        } while (lastLine.size() >= 4 && lastLine[3] == '-');
        response = lastLine;  // 保留最后一行
    }
    return code;
}

// Base64 编码
std::string base64Encode(const std::string& input) {
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "abcdefghijklmnopqrstuvwxyz"
                                "0123456789+/";
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);
    for (size_t i = 0; i < input.size(); i += 3) {
        uint32_t val = static_cast<unsigned char>(input[i]) << 16;
        if (i + 1 < input.size()) val |= static_cast<unsigned char>(input[i+1]) << 8;
        if (i + 2 < input.size()) val |= static_cast<unsigned char>(input[i+2]);
        out += table[(val >> 18) & 0x3F];
        out += table[(val >> 12) & 0x3F];
        out += (i + 1 < input.size()) ? table[(val >> 6) & 0x3F] : '=';
        out += (i + 2 < input.size()) ? table[val & 0x3F] : '=';
    }
    return out;
}

} // anonymous namespace

EmailSender::SmtpConfig EmailSender::loadConfig() {
    SmtpConfig cfg;
    const char* host = std::getenv("GOCOOK_SMTP_HOST");
    const char* port = std::getenv("GOCOOK_SMTP_PORT");
    const char* user = std::getenv("GOCOOK_SMTP_USER");
    const char* pass = std::getenv("GOCOOK_SMTP_PASS");
    const char* from = std::getenv("GOCOOK_SMTP_FROM");

    if (host) cfg.host = host;
    if (port) {
        try { cfg.port = std::stoi(port); }
        catch (...) { cfg.port = 465; }
    }
    if (user) cfg.username = user;
    if (pass) cfg.password = pass;
    if (from) cfg.from = from;
    else cfg.from = cfg.username;

    cfg.valid = !cfg.username.empty() && !cfg.password.empty();
    return cfg;
}

bool EmailSender::sendPasswordResetEmail(const std::string& toEmail,
                                          const std::string& token) {
    auto cfg = loadConfig();
    if (!cfg.valid) {
        LOG_WARN("SMTP not configured — password reset token logged to stderr instead of sent");
        return false;
    }

    std::string subject = "GoCook - 密码重置";
    std::ostringstream body;
    body << "<!DOCTYPE html><html><body style='font-family: sans-serif; padding: 20px;'>"
         << "<h2>GoCook 密码重置</h2>"
         << "<p>您好，</p>"
         << "<p>您最近请求重置 GoCook 账户密码。请使用以下重置令牌：</p>"
         << "<div style='margin: 20px 0; padding: 12px; background: #f5f5f5; "
         << "     border-radius: 6px; font-family: monospace; font-size: 16px; "
         << "     text-align: center; letter-spacing: 2px;'>"
         << token << "</div>"
         << "<p>此令牌将在 <strong>1 小时</strong> 后过期。</p>"
         << "<p>如果您没有请求重置密码，请忽略此邮件。</p>"
         << "<hr><p style='color: #888; font-size: 12px;'>GoCook 团队</p>"
         << "</body></html>";

    return sendEmail(toEmail, subject, body.str());
}

bool EmailSender::sendEmail(const std::string& to,
                             const std::string& subject,
                             const std::string& body) {
    auto cfg = loadConfig();
    if (!cfg.valid) {
        LOG_WARN("SMTP not configured, cannot send email");
        return false;
    }
    return sendRaw(cfg, to, subject, body);
}

bool EmailSender::sendRaw(SmtpConfig& cfg, const std::string& to,
                           const std::string& subject, const std::string& body) {
    // Initialize OpenSSL
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();

    // Create SSL context
    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        LOG_ERROR("Failed to create SSL context");
        return false;
    }

    // Set verification mode — skip cert verification for simplicity;
    // in production, set CA path and enable verification
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);

    BIO* bio = BIO_new_ssl_connect(ctx);
    if (!bio) {
        LOG_ERROR("Failed to create BIO");
        SSL_CTX_free(ctx);
        return false;
    }

    // Set SSL pointer
    SSL* ssl = nullptr;
    BIO_get_ssl(bio, &ssl);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

    // Connect to SMTP server
    std::string addr = cfg.host + ":" + std::to_string(cfg.port);
    BIO_set_conn_hostname(bio, addr.c_str());

    if (BIO_do_connect(bio) <= 0) {
        LOG_ERROR("Failed to connect to SMTP server %s", addr.c_str());
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    // Perform TLS handshake
    if (BIO_do_handshake(bio) <= 0) {
        LOG_ERROR("SSL handshake failed with %s", addr.c_str());
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return false;
    }

    std::string resp;
    bool ok = true;

    // Read greeting
    sendCommand(bio, "", resp);
    LOG_DEBUG("SMTP: %s", resp.c_str());

    // EHLO（排干多行响应）
    std::string ehlo = "EHLO gocook-server";
    if (sendCommand(bio, ehlo, resp, true) != 250) {
        LOG_ERROR("EHLO failed: %s", resp.c_str());
        ok = false;
    }

    // AUTH LOGIN
    if (ok) {
        if (sendCommand(bio, "AUTH LOGIN", resp) != 334) {
            LOG_ERROR("AUTH LOGIN failed: %s", resp.c_str());
            ok = false;
        }
    }

    // Send username (base64)
    if (ok) {
        if (sendCommand(bio, base64Encode(cfg.username), resp) != 334) {
            LOG_ERROR("AUTH username failed: %s", resp.c_str());
            ok = false;
        }
    }

    // Send password (base64)
    if (ok) {
        if (sendCommand(bio, base64Encode(cfg.password), resp) != 235) {
            LOG_ERROR("AUTH password failed: %s", resp.c_str());
            ok = false;
        }
    }

    // MAIL FROM
    if (ok) {
        if (sendCommand(bio, "MAIL FROM:<" + cfg.from + ">", resp) != 250) {
            LOG_ERROR("MAIL FROM failed: %s", resp.c_str());
            ok = false;
        }
    }

    // RCPT TO
    if (ok) {
        if (sendCommand(bio, "RCPT TO:<" + to + ">", resp) != 250) {
            LOG_ERROR("RCPT TO failed: %s", resp.c_str());
            ok = false;
        }
    }

    // DATA
    if (ok) {
        if (sendCommand(bio, "DATA", resp) != 354) {
            LOG_ERROR("DATA failed: %s", resp.c_str());
            ok = false;
        }
    }

    // Send email content
    if (ok) {
        std::ostringstream email;
        email << "From: " << cfg.from << "\r\n"
              << "To: " << to << "\r\n"
              << "Subject: =?UTF-8?B?" << base64Encode(subject) << "?=\r\n"
              << "MIME-Version: 1.0\r\n"
              << "Content-Type: text/html; charset=UTF-8\r\n"
              << "Content-Transfer-Encoding: base64\r\n"
              << "\r\n"
              << base64Encode(body) << "\r\n"
              << ".\r\n";

        std::string emailStr = email.str();
        BIO_write(bio, emailStr.data(), emailStr.size());
        BIO_flush(bio);
        resp = readLine(bio);
        if (resp.empty() || resp.substr(0, 3) != "250") {
            LOG_ERROR("DATA send failed: %s", resp.c_str());
            ok = false;
        }
    }

    // QUIT
    sendCommand(bio, "QUIT", resp);

    // Cleanup
    BIO_free_all(bio);
    SSL_CTX_free(ctx);

    if (ok) {
        LOG_INFO("Password reset email sent to %s", to.c_str());
    }
    return ok;
}
