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
    int code = 0;
    try {
        code = (response.size() >= 3) ? std::stoi(response.substr(0, 3)) : -1;
    } catch (const std::exception& e) {
        LOG_ERROR("Invalid SMTP response code: '%s' — %s", response.c_str(), e.what());
        return -1;
    }
    // 如果响应行是 NNN- 开头（多行响应），继续读取直到 NNN 空格 开头
    if (drainMultiline && response.size() >= 4 && response[3] == '-') {
        std::string lastLine;
        int maxLines = 50;
        do {
            lastLine = readLine(bio);
            if (--maxLines <= 0) {
                LOG_WARN("SMTP multiline response exceeded 50 lines — truncating");
                break;
            }
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
        catch (const std::exception&) {
            LOG_WARN("SMTP_PORT '%s' invalid, falling back to 587", port);
            cfg.port = 587;
        }
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
         << "<div style='margin: 20px 0; padding: 24px 12px; background: #f5f5f5; "
         << "     border-radius: 8px; font-family: monospace; font-size: 56px; font-weight: bold; "
         << "     text-align: center; letter-spacing: 8px;'>"
         << token << "</div>"
         << "<p>此验证码将在 <strong>15 分钟</strong> 后过期。</p>"
         << "<p>如果您没有请求重置密码，请忽略此邮件。</p>"
         << "<hr><p style='color: #888; font-size: 12px;'>GoCook 团队</p>"
         << "</body></html>";

    return sendRaw(cfg, toEmail, subject, body.str());
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

bool EmailSender::isConfigured() {
    return loadConfig().valid;
}

/// @brief 创建配置了证书校验的 SSL_CTX。
///        失败时返回 nullptr；调用方必须调用 SSL_CTX_free 释放。
static SSL_CTX* createSmtpSslCtx() {
    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        LOG_ERROR("Failed to create SSL context");
        return nullptr;
    }

    const char* insecureEnv = std::getenv("GOCOOK_SMTP_TLS_INSECURE");
    bool tlsInsecure = (insecureEnv && (std::string(insecureEnv) == "true"
                                         || std::string(insecureEnv) == "1"));
    if (tlsInsecure) {
        LOG_WARN("TLS verification DISABLED via GOCOOK_SMTP_TLS_INSECURE");
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    } else {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
        if (!SSL_CTX_set_default_verify_paths(ctx)) {
            LOG_WARN("Failed to load default CA paths — verification may fail");
        }
    }
    return ctx;
}

/// @brief 通过 STARTTLS（端口 587）建立连接：明文 TCP → EHLO → STARTTLS → TLS 握手 → 重新 EHLO。
///        成功时返回 BIO 链（TCP 之上的 SSL）；失败返回 nullptr。
///        调用方必须对返回的链调用 BIO_free_all 释放。
static BIO* startTlsConnect(SSL_CTX* ctx, const std::string& addr) {
    // 1. 建立明文 TCP 连接
    BIO* tcpBio = BIO_new_connect(addr.c_str());
    if (!tcpBio) return nullptr;
    if (BIO_do_connect(tcpBio) <= 0) {
        BIO_free_all(tcpBio);
        return nullptr;
    }

    // 2. 读取服务器问候
    std::string resp;
    sendCommand(tcpBio, "", resp);
    LOG_DEBUG("SMTP greeting: %s", resp.c_str());

    // 3. 发送 EHLO（排空多行响应）
    if (sendCommand(tcpBio, "EHLO gocook-server", resp, true) != 250) {
        LOG_ERROR("EHLO failed: %s", resp.c_str());
        BIO_free_all(tcpBio);
        return nullptr;
    }

    // 4. 发送 STARTTLS
    if (sendCommand(tcpBio, "STARTTLS", resp) != 220) {
        LOG_ERROR("STARTTLS failed: %s", resp.c_str());
        BIO_free_all(tcpBio);
        return nullptr;
    }

    // 5. 用 SSL 包裹 TCP BIO
    BIO* sslBio = BIO_new_ssl(ctx, 1);
    if (!sslBio) {
        BIO_free_all(tcpBio);
        return nullptr;
    }
    SSL* ssl = nullptr;
    BIO_get_ssl(sslBio, &ssl);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

    // 链：sslBio → tcpBio
    BIO* chain = BIO_push(sslBio, tcpBio);

    // 6. 在既有 TCP 连接上进行 TLS 握手
    if (BIO_do_handshake(chain) <= 0) {
        LOG_ERROR("STARTTLS handshake failed");
        BIO_free_all(chain);
        return nullptr;
    }

    // 7. 在 TLS 内重新 EHLO（RFC 3207 要求）
    if (sendCommand(chain, "EHLO gocook-server", resp, true) != 250) {
        LOG_ERROR("EHLO inside TLS failed: %s", resp.c_str());
        BIO_free_all(chain);
        return nullptr;
    }

    return chain;
}

/// @brief 在已建立的 TLS BIO 上发送 SMTP AUTH LOGIN、MAIL FROM、RCPT TO、DATA 与 QUIT。
///        邮件被接受（DATA 返回 250）时返回 true。
static bool smtpSendMail(BIO* bio,
                          const std::string& username,
                          const std::string& password,
                          const std::string& from,
                          const std::string& to,
                          const std::string& subject,
                          const std::string& body) {
    std::string resp;

    // 发送 AUTH LOGIN
    if (sendCommand(bio, "AUTH LOGIN", resp) != 334) {
        LOG_ERROR("AUTH LOGIN failed: %s", resp.c_str());
        return false;
    }
    // 用户名（base64 编码）
    if (sendCommand(bio, base64Encode(username), resp) != 334) {
        LOG_ERROR("AUTH username failed: %s", resp.c_str());
        return false;
    }
    // 密码（base64 编码）
    if (sendCommand(bio, base64Encode(password), resp) != 235) {
        LOG_ERROR("AUTH password failed: %s", resp.c_str());
        return false;
    }
    // 发送 MAIL FROM
    if (sendCommand(bio, "MAIL FROM:<" + from + ">", resp) != 250) {
        LOG_ERROR("MAIL FROM failed: %s", resp.c_str());
        return false;
    }
    // 发送 RCPT TO
    if (sendCommand(bio, "RCPT TO:<" + to + ">", resp) != 250) {
        LOG_ERROR("RCPT TO failed: %s", resp.c_str());
        return false;
    }
    // 发送 DATA
    if (sendCommand(bio, "DATA", resp) != 354) {
        LOG_ERROR("DATA failed: %s", resp.c_str());
        return false;
    }

    // 发送邮件内容
    std::ostringstream email;
    email << "From: " << from << "\r\n"
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
        return false;
    }

    // 发送 QUIT
    sendCommand(bio, "QUIT", resp);
    return true;
}

bool EmailSender::sendRaw(SmtpConfig& cfg, const std::string& to,
                           const std::string& subject, const std::string& body) {
    // 初始化 OpenSSL — OPENSSL_init_ssl 负责 OpenSSL 3.x 的全部初始化
    // （SSL_load_error_strings / ERR_load_BIO_strings / OpenSSL_add_all_algorithms
    //  在 OpenSSL 3.0 中已弃用，init_ssl 会自动调用）
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);

    // 创建带证书校验的 SSL 上下文
    SSL_CTX* ctx = createSmtpSslCtx();
    if (!ctx) return false;

    std::string addr = cfg.host + ":" + std::to_string(cfg.port);
    bool ok = false;

    if (cfg.port == 587) {
        // STARTTLS 模式：明文 TCP → EHLO → STARTTLS → TLS → 重新 EHLO
        BIO* chain = startTlsConnect(ctx, addr);
        if (chain) {
            ok = smtpSendMail(chain, cfg.username, cfg.password, cfg.from,
                              to, subject, body);
            BIO_free_all(chain);
        } else {
            LOG_ERROR("STARTTLS connection failed for %s", addr.c_str());
        }
    } else {
        // 隐式 TLS 模式（465+）：连接与握手一步完成
        BIO* bio = BIO_new_ssl_connect(ctx);
        if (!bio) {
            LOG_ERROR("Failed to create SSL BIO");
            SSL_CTX_free(ctx);
            return false;
        }

        SSL* ssl = nullptr;
        BIO_get_ssl(bio, &ssl);
        SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
        BIO_set_conn_hostname(bio, addr.c_str());

        if (BIO_do_connect(bio) <= 0) {
            LOG_ERROR("Failed to connect to %s", addr.c_str());
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }
        if (BIO_do_handshake(bio) <= 0) {
            LOG_ERROR("TLS handshake failed with %s", addr.c_str());
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return false;
        }

        // 读取问候 + EHLO（排空多行）
        std::string resp;
        sendCommand(bio, "", resp);
        LOG_DEBUG("SMTP greeting: %s", resp.c_str());
        if (sendCommand(bio, "EHLO gocook-server", resp, true) == 250) {
            ok = smtpSendMail(bio, cfg.username, cfg.password, cfg.from,
                              to, subject, body);
        } else {
            LOG_ERROR("EHLO failed: %s", resp.c_str());
        }

        BIO_free_all(bio);
    }

    SSL_CTX_free(ctx);

    if (ok) {
        LOG_INFO("Password reset email sent to %s", to.c_str());
    }
    return ok;
}
