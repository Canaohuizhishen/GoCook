-- ==================== GoCook 测试数据初始化脚本 ====================
-- 说明：本脚本可重复执行，不会造成数据重复。
-- 执行方式：docker exec -i my_postgres psql -U gocook -d gocookdb < ./seed_test_data.sql

-- 1. 插入测试用户（若已存在则不做任何操作）
INSERT INTO users (username, password_hash, email, phone, avatar_url)
VALUES ('testuser', 'test123', 'test@example.com', '138****1234', '')
ON CONFLICT (username) DO NOTHING;

-- 2. 插入示例菜谱（指定固定 id，冲突时更新，新增 author_id 列）
INSERT INTO recipes (id, name, description, ingredients, instructions, prep_time_minutes, cook_time_minutes, servings, tags, author_id)
VALUES 
    (1, '番茄炒蛋', '经典家常菜',
     '[{"name":"番茄","quantity":2,"unit":"个"},{"name":"鸡蛋","quantity":3,"unit":"个"},{"name":"盐","quantity":5,"unit":"克"},{"name":"糖","quantity":3,"unit":"克"}]',
     '1. 打散鸡蛋；2. 炒鸡蛋盛出；3. 炒番茄；4. 混合调味。', 5, 10, 2, '{"中式","快手"}', 1),
    (2, '清炒西兰花', '健康低脂',
     '[{"name":"西兰花","quantity":1,"unit":"颗"},{"name":"蒜","quantity":3,"unit":"瓣"},{"name":"盐","quantity":3,"unit":"克"}]',
     '1. 焯水西兰花；2. 爆香蒜；3. 翻炒调味。', 5, 5, 2, '{"低卡","素食"}', 1),
    (3, '鸡胸肉沙拉', '高蛋白轻食',
     '[{"name":"鸡胸肉","quantity":1,"unit":"块"},{"name":"生菜","quantity":3,"unit":"片"},{"name":"小番茄","quantity":5,"unit":"个"},{"name":"橄榄油","quantity":10,"unit":"毫升"},{"name":"黑胡椒","quantity":1,"unit":"克"}]',
     '1. 鸡胸肉煮熟撕成丝；2. 蔬菜洗净切好；3. 混合淋上橄榄油和黑胡椒。', 10, 10, 1, '{"轻食","高蛋白"}', 1)
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name,
    description = EXCLUDED.description,
    ingredients = EXCLUDED.ingredients,
    instructions = EXCLUDED.instructions,
    prep_time_minutes = EXCLUDED.prep_time_minutes,
    cook_time_minutes = EXCLUDED.cook_time_minutes,
    servings = EXCLUDED.servings,
    tags = EXCLUDED.tags,
    author_id = EXCLUDED.author_id,
    updated_at = NOW();

-- 3. 为测试用户添加/更新库存数据（动态获取 user_id）
INSERT INTO inventory (user_id, ingredient_name, quantity, unit, expiry_date)
SELECT 
    (SELECT id FROM users WHERE username = 'testuser'),
    v.ingredient_name,
    v.quantity,
    v.unit,
    v.expiry_date
FROM (VALUES
    ('鸡蛋', 6, '个', '2026-04-20'::DATE),
    ('番茄', 3, '个', '2026-04-15'::DATE),
    ('西兰花', 1, '颗', '2026-04-18'::DATE),
    ('大蒜', 5, '瓣', NULL),
    ('牛奶', 500, '毫升', '2026-04-12'::DATE),
    ('鸡胸肉', 300, '克', '2026-04-22'::DATE),
    ('大米', 2.5, '公斤', NULL),
    ('盐', 1, '袋', NULL),
    ('食用油', 900, '毫升', '2026-10-01'::DATE),
    ('生菜', 2, '颗', '2026-04-14'::DATE),
    ('小番茄', 200, '克', '2026-04-14'::DATE)
) AS v(ingredient_name, quantity, unit, expiry_date)
ON CONFLICT (user_id, ingredient_name) DO UPDATE SET
    quantity = EXCLUDED.quantity,
    unit = EXCLUDED.unit,
    expiry_date = EXCLUDED.expiry_date,
    added_at = NOW();

-- 4. 插入用户偏好（冲突时忽略）
INSERT INTO user_preferences (user_id, preference_type, value)
SELECT 
    (SELECT id FROM users WHERE username = 'testuser'),
    v.pref_type,
    v.val
FROM (VALUES
    ('likes', '中式'),
    ('dislikes', '香菜'),
    ('allergies', '花生'),
    ('health_goal', '高蛋白')
) AS v(pref_type, val)
ON CONFLICT (user_id, preference_type, value) DO NOTHING;

-- 完成提示
\echo 'Test data seeded successfully!'