-- ==================== GoCook 数据库表结构初始化脚本 ====================
-- 说明：本脚本可安全重复执行，已存在的对象不会被重复创建或报错。
-- 执行方式：docker exec -i my_postgres psql -U gocook -d gocookdb < ./create_all_tables.sql

-- 1. 菜谱表
CREATE TABLE IF NOT EXISTS recipes (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    ingredients TEXT[] DEFAULT '{}',
    instructions TEXT,
    prep_time_minutes INT,
    cook_time_minutes INT,
    servings INT,
    image_url TEXT,
    nutrition_info JSONB,
    tags TEXT[],
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- 为菜谱名称创建索引（如果不存在）
CREATE INDEX IF NOT EXISTS idx_recipes_name ON recipes (name);

-- 2. 用户表
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT NOW()
);

-- 3. 用户偏好与禁忌表
CREATE TABLE IF NOT EXISTS user_preferences (
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    preference_type TEXT CHECK (preference_type IN ('likes', 'dislikes', 'allergies', 'health_goal')),
    value TEXT,
    PRIMARY KEY (user_id, preference_type, value)
);

-- 4. 用户食材库存表
CREATE TABLE IF NOT EXISTS inventory (
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    ingredient_name TEXT,
    quantity DECIMAL(10,2),
    unit TEXT,
    expiry_date DATE,
    added_at TIMESTAMP DEFAULT NOW(),
    PRIMARY KEY (user_id, ingredient_name)
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