-- ==================== GoCook 测试数据初始化脚本 ====================
-- 说明：本脚本可重复执行，不会造成数据重复。
-- 执行方式：docker exec -i my_postgres psql -U gocook -d gocookdb < ./seed_test_data.sql

-- 1. 插入测试用户（若已存在则不做任何操作）
-- 密码：test123
INSERT INTO users (username, password_hash, display_name, email, phone, avatar_url, preferences_complete, role, status)
VALUES ('testuser', '$2a$10$AabeJArJr8/VkhmM9kWaxe4qc01p54NXg7QVAYYAVq7I136EVp1T6', '测试厨师', 'test@example.com', '138****1234', '', TRUE, 'super_admin', 'active')
ON CONFLICT (username) DO UPDATE SET
    display_name = EXCLUDED.display_name,
    email = EXCLUDED.email,
    phone = EXCLUDED.phone,
    preferences_complete = EXCLUDED.preferences_complete,
    role = EXCLUDED.role,
    status = EXCLUDED.status;

-- 2. 插入示例菜谱（指定固定 id，冲突时更新）
INSERT INTO recipes (id, name, description, ingredients, instructions, steps, prep_time_minutes, cook_time_minutes, servings, tags, cooking_method, flavor, ingredient_type, status, author_id)
VALUES 
    (1, '番茄炒蛋', '经典家常菜',
     '[{"name":"番茄","quantity":2,"unit":"个"},{"name":"鸡蛋","quantity":3,"unit":"个"},{"name":"盐","quantity":5,"unit":"克"},{"name":"糖","quantity":3,"unit":"克"}]',
     '1. 打散鸡蛋；2. 炒鸡蛋盛出；3. 炒番茄；4. 混合调味。',
     '[{"order":1,"description":"鸡蛋打散，加少许盐搅拌均匀。","duration":60},{"order":2,"description":"番茄切块备用。","duration":120},{"order":3,"description":"热锅放油，倒入蛋液炒熟盛出。","duration":180},{"order":4,"description":"锅中留底油，放入番茄炒出汁，加入鸡蛋翻炒均匀。","duration":240}]',
     5, 10, 2, '{"中式","快手"}', '炒', '清淡', '荤', 'approved', 1),
    (2, '清炒西兰花', '健康低脂',
     '[{"name":"西兰花","quantity":1,"unit":"颗"},{"name":"蒜","quantity":3,"unit":"瓣"},{"name":"盐","quantity":3,"unit":"克"}]',
     '1. 焯水西兰花；2. 爆香蒜；3. 翻炒调味。',
     '[{"order":1,"description":"西兰花掰小朵，焯水后沥干。","duration":120},{"order":2,"description":"蒜切末，热锅放油爆香。","duration":60},{"order":3,"description":"放入西兰花翻炒，加盐调味。","duration":180}]',
     5, 5, 2, '{"低卡","素食"}', '炒', '清淡', '素', 'approved', 1),
    (3, '鸡胸肉沙拉', '高蛋白轻食',
     '[{"name":"鸡胸肉","quantity":1,"unit":"块"},{"name":"生菜","quantity":3,"unit":"片"},{"name":"小番茄","quantity":5,"unit":"个"},{"name":"橄榄油","quantity":10,"unit":"毫升"},{"name":"黑胡椒","quantity":1,"unit":"克"}]',
     '1. 鸡胸肉煮熟撕成丝；2. 蔬菜洗净切好；3. 混合淋上橄榄油和黑胡椒。',
     '[{"order":1,"description":"鸡胸肉煮熟，撕成细丝。","duration":600},{"order":2,"description":"生菜、小番茄洗净，生菜撕片，小番茄对切。","duration":180},{"order":3,"description":"所有食材放入大碗，淋橄榄油，撒黑胡椒拌匀。","duration":60}]',
     10, 10, 1, '{"轻食","高蛋白"}', '拌', '清淡', '荤', 'approved', 1)
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name,
    description = EXCLUDED.description,
    ingredients = EXCLUDED.ingredients,
    instructions = EXCLUDED.instructions,
    steps = EXCLUDED.steps,
    prep_time_minutes = EXCLUDED.prep_time_minutes,
    cook_time_minutes = EXCLUDED.cook_time_minutes,
    servings = EXCLUDED.servings,
    tags = EXCLUDED.tags,
    cooking_method = EXCLUDED.cooking_method,
    flavor = EXCLUDED.flavor,
    ingredient_type = EXCLUDED.ingredient_type,
    status = EXCLUDED.status,
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
    ('dislikes', '花生'),
    ('health_goal', '高蛋白')
) AS v(pref_type, val)
ON CONFLICT (user_id, preference_type, value) DO NOTHING;

-- 5. 插入健康指标
INSERT INTO health_profiles (user_id, height_cm, weight_kg, conditions)
VALUES (
    (SELECT id FROM users WHERE username = 'testuser'),
    175,
    70,
    '["高血压"]'::jsonb
)
ON CONFLICT (user_id) DO UPDATE SET
    height_cm = EXCLUDED.height_cm,
    weight_kg = EXCLUDED.weight_kg,
    conditions = EXCLUDED.conditions,
    updated_at = NOW();

-- 6. 创建收藏分组
INSERT INTO favorite_groups (id, user_id, name, sort_order) VALUES
    (1, (SELECT id FROM users WHERE username = 'testuser'), '家常菜', 1),
    (2, (SELECT id FROM users WHERE username = 'testuser'), '低卡轻食', 2)
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name,
    sort_order = EXCLUDED.sort_order;

-- 7. 添加收藏菜谱
INSERT INTO favorites (user_id, recipe_id, group_id, is_public) VALUES
    ((SELECT id FROM users WHERE username = 'testuser'), 1, 1, TRUE),
    ((SELECT id FROM users WHERE username = 'testuser'), 2, 2, FALSE)
ON CONFLICT (user_id, recipe_id) DO UPDATE SET
    group_id = EXCLUDED.group_id,
    is_public = EXCLUDED.is_public;

-- 8. 添加评分与评论
INSERT INTO ratings (user_id, recipe_id, rating, comment) VALUES
    ((SELECT id FROM users WHERE username = 'testuser'), 1, 5, '简单易做，味道好极了！')
ON CONFLICT (user_id, recipe_id) DO UPDATE SET
    rating = EXCLUDED.rating,
    comment = EXCLUDED.comment,
    updated_at = NOW();

-- 9. 添加系统公告
INSERT INTO announcements (id, title, content) VALUES
    (1, '系统维护通知', '今晚 22:00-24:00 进行系统升级，届时服务不可用。')
ON CONFLICT (id) DO UPDATE SET
    title = EXCLUDED.title,
    content = EXCLUDED.content;

-- 10. 添加通知示例
INSERT INTO notifications (id, user_id, title, content, type, sub_type, related_id, trigger_user_name, is_read) VALUES
    (1, (SELECT id FROM users WHERE username = 'testuser'), '审核结果', '您的菜谱已通过审核，现在可以在首页看到啦！', 'review', NULL, 1, NULL, FALSE),
    (2, (SELECT id FROM users WHERE username = 'testuser'), '新的互动', '用户 foodie_lily 回复了你的评论', 'interaction', 'comment_reply', 1, 'foodie_lily', FALSE)
ON CONFLICT (id) DO UPDATE SET
    title = EXCLUDED.title,
    content = EXCLUDED.content,
    type = EXCLUDED.type,
    sub_type = EXCLUDED.sub_type,
    related_id = EXCLUDED.related_id,
    trigger_user_name = EXCLUDED.trigger_user_name,
    is_read = EXCLUDED.is_read;

-- 11. 创建购物清单
INSERT INTO shopping_lists (id, user_id, name) VALUES
    (1, (SELECT id FROM users WHERE username = 'testuser'), '周末聚餐清单')
ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name;

-- 12. 添加购物清单项
INSERT INTO shopping_list_items (list_id, ingredient_name, required_quantity, inventory_quantity, to_buy_quantity, unit, checked)
SELECT 1, v.ingredient_name, v.required_quantity, v.inventory_quantity, v.to_buy_quantity, v.unit, v.checked
FROM (VALUES
    ('西兰花', 2, 0, 2, '颗', FALSE),
    ('鸡蛋', 4, 2, 2, '个', FALSE),
    ('葱花', 1, 0, 1, '把', FALSE)
) AS v(ingredient_name, required_quantity, inventory_quantity, to_buy_quantity, unit, checked)
WHERE NOT EXISTS (SELECT 1 FROM shopping_list_items WHERE list_id = 1 AND ingredient_name = v.ingredient_name);

-- 13. 添加膳食计划
INSERT INTO meal_plans (user_id, recipe_id, date, meal_type) VALUES
    ((SELECT id FROM users WHERE username = 'testuser'), 1, '2026-05-13', 'lunch'),
    ((SELECT id FROM users WHERE username = 'testuser'), 2, '2026-05-13', 'dinner')
ON CONFLICT DO NOTHING;

-- 完成提示
\echo 'Test data seeded successfully!'