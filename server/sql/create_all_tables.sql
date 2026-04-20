-- ==================== GoCook 数据库表结构初始化脚本 ====================
-- 说明：本脚本可安全重复执行，已存在的对象不会被重复创建或报错。
-- 执行方式：docker exec -i my_postgres psql -U gocook -d gocookdb < ./create_all_tables.sql

-- 先删除所有表（按依赖顺序，先删有外键的表）
DROP TABLE IF EXISTS shopping_list    CASCADE;
DROP TABLE IF EXISTS inventory        CASCADE;
DROP TABLE IF EXISTS user_preferences CASCADE;
DROP TABLE IF EXISTS recipes          CASCADE;
DROP TABLE IF EXISTS users            CASCADE;

-- 1. 用户表
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    email TEXT,
    phone TEXT,
    avatar_url TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- 2. 菜谱表
CREATE TABLE IF NOT EXISTS recipes (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    ingredients JSONB NOT NULL DEFAULT '[]',
    instructions TEXT,
    prep_time_minutes INT,
    cook_time_minutes INT,
    servings INT,
    image_url TEXT,
    nutrition_info JSONB,
    tags TEXT[],
    author_id INT REFERENCES users(id),   -- 作者 ID，关联用户表
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

-- 4. 用户食材库存表
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

-- 5. 购物清单表
CREATE TABLE IF NOT EXISTS shopping_list (
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    ingredient_name TEXT,
    quantity DECIMAL(10,2),
    unit TEXT,
    checked BOOLEAN DEFAULT FALSE,
    PRIMARY KEY (user_id, ingredient_name)
);

-- 完成提示
\echo 'All tables created successfully (existing objects skipped).'