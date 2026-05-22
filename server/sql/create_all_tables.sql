-- ==================== GoCook 数据库表结构初始化脚本 ====================
-- 说明：本脚本可安全重复执行，已存在的对象不会被重复创建或报错。
-- 执行方式：docker exec -i my_postgres psql -U gocook -d gocookdb < ./create_all_tables.sql

-- 先删除所有表（按依赖顺序，先删有外键的表）
DROP TABLE IF EXISTS activity_logs          CASCADE;
DROP TABLE IF EXISTS admin_logs             CASCADE;
DROP TABLE IF EXISTS notifications          CASCADE;
DROP TABLE IF EXISTS ratings                CASCADE;
DROP TABLE IF EXISTS favorites              CASCADE;
DROP TABLE IF EXISTS favorite_groups        CASCADE;
DROP TABLE IF EXISTS meal_plans             CASCADE;
DROP TABLE IF EXISTS shopping_list_items    CASCADE;
DROP TABLE IF EXISTS shopping_lists         CASCADE;
DROP TABLE IF EXISTS inventory              CASCADE;
DROP TABLE IF EXISTS password_reset_tokens   CASCADE;
DROP TABLE IF EXISTS health_profiles        CASCADE;
DROP TABLE IF EXISTS user_preferences       CASCADE;
DROP TABLE IF EXISTS recipes                CASCADE;
DROP TABLE IF EXISTS announcements          CASCADE;
DROP TABLE IF EXISTS users                  CASCADE;

-- 1. 用户表
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    display_name TEXT,
    email TEXT UNIQUE,
    phone TEXT,
    avatar_url TEXT,
    preferences_complete BOOLEAN DEFAULT FALSE,
    role TEXT DEFAULT 'user' CHECK (role IN ('user', 'moderator', 'super_admin')),
    status TEXT DEFAULT 'active' CHECK (status IN ('active', 'frozen')),
    created_at TIMESTAMP DEFAULT NOW()
);

-- 2. 菜谱表
CREATE TABLE IF NOT EXISTS recipes (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    ingredients JSONB NOT NULL DEFAULT '[]',
    instructions TEXT,
    steps JSONB DEFAULT '[]',
    prep_time_minutes INT,
    cook_time_minutes INT,
    servings INT,
    image_url TEXT,
    nutrition_info JSONB,
    tags TEXT[],
    cooking_method TEXT,
    flavor TEXT,
    ingredient_type TEXT,
    view_count INT DEFAULT 0,
    avg_rating DECIMAL(2,1) DEFAULT 0.0,
    status TEXT DEFAULT 'pending' CHECK (status IN ('pending', 'approved', 'rejected')),
    reject_reason TEXT,
    author_id INT REFERENCES users(id),
    submitted_at TIMESTAMP DEFAULT NOW(),
    reviewed_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- 为菜谱名称创建索引（如果不存在）
CREATE INDEX IF NOT EXISTS idx_recipes_name ON recipes (name);

-- 3. 用户偏好与禁忌表
CREATE TABLE IF NOT EXISTS user_preferences (
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    preference_type TEXT CHECK (preference_type IN ('likes', 'dislikes', 'allergies', 'health_goal')),
    value TEXT,
    PRIMARY KEY (user_id, preference_type, value)
);

-- 4. 用户健康指标表
CREATE TABLE IF NOT EXISTS health_profiles (
    user_id INT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    height_cm INT,
    weight_kg DECIMAL(5,1),
    conditions JSONB DEFAULT '[]',
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- 5. 用户食材库存表
CREATE TABLE IF NOT EXISTS inventory (
    id SERIAL PRIMARY KEY,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    ingredient_name TEXT NOT NULL,
    quantity DECIMAL(10,2),
    unit TEXT,
    expiry_date DATE,
    added_at TIMESTAMP DEFAULT NOW(),
    UNIQUE (user_id, ingredient_name)
);

-- 6. 收藏分组表
CREATE TABLE IF NOT EXISTS favorite_groups (
    id SERIAL PRIMARY KEY,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    sort_order INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT NOW()
);

-- 7. 用户收藏表
CREATE TABLE IF NOT EXISTS favorites (
    id SERIAL PRIMARY KEY,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    recipe_id INT NOT NULL REFERENCES recipes(id) ON DELETE CASCADE,
    group_id INT REFERENCES favorite_groups(id) ON DELETE SET NULL,
    is_public BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT NOW(),
    UNIQUE (user_id, recipe_id)
);

-- 8. 菜谱评分与评论表
CREATE TABLE IF NOT EXISTS ratings (
    id SERIAL PRIMARY KEY,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    recipe_id INT NOT NULL REFERENCES recipes(id) ON DELETE CASCADE,
    rating INT NOT NULL CHECK (rating >= 1 AND rating <= 5),
    comment TEXT,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),
    UNIQUE (user_id, recipe_id)
);

-- 9. 通知表
CREATE TABLE IF NOT EXISTS notifications (
    id SERIAL PRIMARY KEY,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    title TEXT,
    content TEXT,
    type TEXT DEFAULT 'system',
    sub_type TEXT,
    is_read BOOLEAN DEFAULT FALSE,
    related_id INT,
    trigger_user_name TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- 10. 膳食计划表
CREATE TABLE IF NOT EXISTS meal_plans (
    id SERIAL PRIMARY KEY,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    recipe_id INT NOT NULL REFERENCES recipes(id) ON DELETE CASCADE,
    date DATE NOT NULL,
    meal_type TEXT NOT NULL CHECK (meal_type IN ('breakfast', 'lunch', 'dinner', 'snack')),
    created_at TIMESTAMP DEFAULT NOW()
);

-- 11. 购物清单主表
CREATE TABLE IF NOT EXISTS shopping_lists (
    id SERIAL PRIMARY KEY,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    plan_id TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- 12. 购物清单项表
CREATE TABLE IF NOT EXISTS shopping_list_items (
    id SERIAL PRIMARY KEY,
    list_id INT NOT NULL REFERENCES shopping_lists(id) ON DELETE CASCADE,
    ingredient_name TEXT NOT NULL,
    required_quantity DECIMAL(10,2) DEFAULT 0,
    inventory_quantity DECIMAL(10,2) DEFAULT 0,
    to_buy_quantity DECIMAL(10,2) DEFAULT 0,
    unit TEXT,
    checked BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT NOW()
);

-- 13. 系统公告表
CREATE TABLE IF NOT EXISTS announcements (
    id SERIAL PRIMARY KEY,
    title TEXT NOT NULL,
    content TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- 14. 管理员操作日志表
CREATE TABLE IF NOT EXISTS admin_logs (
    id SERIAL PRIMARY KEY,
    operator_id INT REFERENCES users(id) ON DELETE SET NULL,
    operator_name TEXT,
    type TEXT,
    action TEXT,
    target_id INT,
    target_name TEXT,
    detail TEXT,
    result TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- 15. 密码重置令牌表
CREATE TABLE IF NOT EXISTS password_reset_tokens (
    id SERIAL PRIMARY KEY,
    user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token TEXT UNIQUE NOT NULL,
    expires_at TIMESTAMPTZ NOT NULL,
    used BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_password_reset_tokens_token ON password_reset_tokens (token);

-- 16. 用户行为日志表
CREATE TABLE IF NOT EXISTS activity_logs (
    id SERIAL PRIMARY KEY,
    user_id INT REFERENCES users(id) ON DELETE SET NULL,
    username TEXT,
    action TEXT,
    target_type TEXT,
    target_id INT,
    detail TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- 完成提示
\echo 'All tables created successfully (existing objects skipped).'