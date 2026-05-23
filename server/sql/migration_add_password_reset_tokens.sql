-- ==========================================================
-- 安全迁移脚本：仅新增 password_reset_tokens 表
-- 不会删除或修改任何已有表/数据，可安全重复执行
-- ==========================================================

CREATE TABLE IF NOT EXISTS password_reset_tokens (
    id SERIAL PRIMARY KEY,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token TEXT UNIQUE NOT NULL,
    expires_at TIMESTAMPTZ NOT NULL,
    used BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_password_reset_tokens_token
    ON password_reset_tokens (token);

\echo 'Migration complete: password_reset_tokens table created (if not existed).'
