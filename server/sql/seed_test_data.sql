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
INSERT INTO recipes (id, name, description, ingredients, instructions, steps, prep_time_minutes, cook_time_minutes, servings, tags, cooking_method, flavor, ingredient_type, status, author_id, nutrition_info)
VALUES 
    (1, '番茄炒蛋', '经典家常菜',
     '[{"name":"番茄","quantity":2,"unit":"个"},{"name":"鸡蛋","quantity":3,"unit":"个"},{"name":"盐","quantity":5,"unit":"克"},{"name":"糖","quantity":3,"unit":"克"}]',
     '1. 打散鸡蛋；2. 炒鸡蛋盛出；3. 炒番茄；4. 混合调味。',
     '[{"order":1,"description":"鸡蛋打散，加少许盐搅拌均匀。","duration":60},{"order":2,"description":"番茄切块备用。","duration":120},{"order":3,"description":"热锅放油，倒入蛋液炒熟盛出。","duration":180},{"order":4,"description":"锅中留底油，放入番茄炒出汁，加入鸡蛋翻炒均匀。","duration":240}]',
     5, 10, 2, '{"中式","快手"}', '炒', '清淡', '荤', 'approved', 1, '{
    "calories": 350,
    "protein": 15,
    "fat": 20,
    "carbs": 30,
    "per_serving": {"calories": 350, "protein_g": 15, "fat_g": 20, "carbs_g": 30, "fiber_g": 2.5, "sodium_mg": 480, "vitamin_c_mg": 25},
    "ingredients_breakdown": [
        {"name": "鸡蛋", "calories": 140, "protein_g": 12, "fat_g": 10, "carbs_g": 2},
        {"name": "番茄", "calories": 40, "protein_g": 2, "fat_g": 0.5, "carbs_g": 8}
    ],
    "health_notes": "本菜谱含钠量适中，高血压用户建议减少额外用盐。"
}'::jsonb),
    (2, '清炒西兰花', '健康低脂',
     '[{"name":"西兰花","quantity":1,"unit":"颗"},{"name":"蒜","quantity":3,"unit":"瓣"},{"name":"盐","quantity":3,"unit":"克"}]',
     '1. 焯水西兰花；2. 爆香蒜；3. 翻炒调味。',
     '[{"order":1,"description":"西兰花掰小朵，焯水后沥干。","duration":120},{"order":2,"description":"蒜切末，热锅放油爆香。","duration":60},{"order":3,"description":"放入西兰花翻炒，加盐调味。","duration":180}]',
     5, 5, 2, '{"低卡","素食"}', '炒', '清淡', '素', 'approved', 1, '{
    "calories": 120,
    "protein": 8,
    "fat": 2,
    "carbs": 18,
    "per_serving": {"calories": 120, "protein_g": 8, "fat_g": 2, "carbs_g": 18, "fiber_g": 6, "sodium_mg": 200, "vitamin_c_mg": 80},
    "ingredients_breakdown": [
        {"name": "西兰花", "calories": 55, "protein_g": 4, "fat_g": 0.5, "carbs_g": 11},
        {"name": "蒜", "calories": 4, "protein_g": 0.2, "fat_g": 0, "carbs_g": 1}
    ],
    "health_notes": "低卡高纤维，适合减脂期食用。"
}'::jsonb),
    (3, '鸡胸肉沙拉', '高蛋白轻食',
     '[{"name":"鸡胸肉","quantity":1,"unit":"块"},{"name":"生菜","quantity":3,"unit":"片"},{"name":"小番茄","quantity":5,"unit":"个"},{"name":"橄榄油","quantity":10,"unit":"毫升"},{"name":"黑胡椒","quantity":1,"unit":"克"}]',
     '1. 鸡胸肉煮熟撕成丝；2. 蔬菜洗净切好；3. 混合淋上橄榄油和黑胡椒。',
     '[{"order":1,"description":"鸡胸肉煮熟，撕成细丝。","duration":600},{"order":2,"description":"生菜、小番茄洗净，生菜撕片，小番茄对切。","duration":180},{"order":3,"description":"所有食材放入大碗，淋橄榄油，撒黑胡椒拌匀。","duration":60}]',
     10, 10, 1, '{"轻食","高蛋白"}', '拌', '清淡', '荤', 'approved', 1, '{
    "calories": 280,
    "protein": 32,
    "fat": 12,
    "carbs": 10,
    "per_serving": {"calories": 280, "protein_g": 32, "fat_g": 12, "carbs_g": 10, "fiber_g": 3, "sodium_mg": 350, "vitamin_c_mg": 15},
    "ingredients_breakdown": [
        {"name": "鸡胸肉", "calories": 165, "protein_g": 31, "fat_g": 3.6, "carbs_g": 0},
        {"name": "生菜", "calories": 5, "protein_g": 0.5, "fat_g": 0, "carbs_g": 1},
        {"name": "小番茄", "calories": 15, "protein_g": 0.7, "fat_g": 0.2, "carbs_g": 3},
        {"name": "橄榄油", "calories": 90, "protein_g": 0, "fat_g": 10, "carbs_g": 0}
    ],
    "health_notes": "高蛋白低脂轻食，适合健身人群。"
}'::jsonb),
    (4, '麻婆豆腐', '经典川菜，麻辣鲜香',
     '[{"name":"嫩豆腐","quantity":1,"unit":"块"},{"name":"猪肉末","quantity":100,"unit":"克"},{"name":"豆瓣酱","quantity":15,"unit":"克"},{"name":"花椒","quantity":2,"unit":"克"},{"name":"蒜","quantity":3,"unit":"瓣"},{"name":"姜","quantity":3,"unit":"片"},{"name":"葱","quantity":2,"unit":"根"},{"name":"淀粉","quantity":10,"unit":"克"}]',
     '1. 豆腐切块焯水；2. 炒肉末和豆瓣酱；3. 加水和豆腐煮3分钟；4. 勾芡撒花椒粉葱花。',
     '[{"order":1,"description":"嫩豆腐切成2厘米方块，沸水中加盐焯烫1分钟后捞出沥干。","duration":120},{"order":2,"description":"热锅放油，下猪肉末炒至变色，加入豆瓣酱、姜蒜末炒出红油。","duration":180},{"order":3,"description":"加入适量清水烧开，放入豆腐块轻轻推匀，中火煮3分钟入味。","duration":180},{"order":4,"description":"水淀粉勾芡，撒花椒粉和葱花，出锅装盘。","duration":60}]',
     5, 10, 2, '{"川菜","麻辣","下饭"}', '烧', '麻辣', '荤', 'approved', 1, '{
    "calories": 420,
    "protein": 22,
    "fat": 28,
    "carbs": 25,
    "per_serving": {"calories": 420, "protein_g": 22, "fat_g": 28, "carbs_g": 25, "fiber_g": 4, "sodium_mg": 900, "vitamin_c_mg": 3},
    "ingredients_breakdown": [
        {"name": "嫩豆腐", "calories": 70, "protein_g": 8, "fat_g": 4, "carbs_g": 3},
        {"name": "猪肉末", "calories": 150, "protein_g": 12, "fat_g": 10, "carbs_g": 0},
        {"name": "豆瓣酱", "calories": 30, "protein_g": 2, "fat_g": 1, "carbs_g": 4}
    ],
    "health_notes": "含钠量较高，高血压患者请谨慎食用。"
}'::jsonb),
    (5, '糖醋里脊', '外酥里嫩，酸甜开胃',
     '[{"name":"猪里脊","quantity":300,"unit":"克"},{"name":"鸡蛋","quantity":1,"unit":"个"},{"name":"淀粉","quantity":50,"unit":"克"},{"name":"番茄酱","quantity":30,"unit":"克"},{"name":"白醋","quantity":15,"unit":"毫升"},{"name":"糖","quantity":20,"unit":"克"},{"name":"盐","quantity":2,"unit":"克"}]',
     '1. 里脊切条腌制；2. 挂糊炸至金黄；3. 炒糖醋汁裹匀。',
     '[{"order":1,"description":"猪里脊切条，加入盐、料酒腌制10分钟。","duration":600},{"order":2,"description":"蛋液加淀粉调成糊，里脊条挂糊后入六成热油锅炸至金黄捞出。","duration":300},{"order":3,"description":"锅中留底油，加入番茄酱、白醋、糖熬至浓稠，倒入炸好的里脊条快速翻炒均匀出锅。","duration":120}]',
     15, 10, 3, '{"酸甜","下饭","宴客"}', '炸', '酸甜', '荤', 'approved', 1, '{
    "calories": 520,
    "protein": 35,
    "fat": 22,
    "carbs": 45,
    "per_serving": {"calories": 520, "protein_g": 35, "fat_g": 22, "carbs_g": 45, "fiber_g": 0, "sodium_mg": 600, "vitamin_c_mg": 1},
    "ingredients_breakdown": [
        {"name": "猪里脊", "calories": 300, "protein_g": 27, "fat_g": 10, "carbs_g": 0},
        {"name": "鸡蛋", "calories": 70, "protein_g": 6, "fat_g": 5, "carbs_g": 0},
        {"name": "淀粉", "calories": 80, "protein_g": 0, "fat_g": 0, "carbs_g": 20}
    ],
    "health_notes": "油炸食品，热量偏高，建议适量食用。"
}'::jsonb),
    (6, '宫保鸡丁', '鸡丁嫩滑，花生酥脆',
     '[{"name":"鸡胸肉","quantity":250,"unit":"克"},{"name":"花生米","quantity":50,"unit":"克"},{"name":"干辣椒","quantity":5,"unit":"个"},{"name":"花椒","quantity":3,"unit":"克"},{"name":"葱","quantity":2,"unit":"根"},{"name":"蒜","quantity":3,"unit":"瓣"},{"name":"酱油","quantity":10,"unit":"毫升"},{"name":"醋","quantity":5,"unit":"毫升"},{"name":"糖","quantity":5,"unit":"克"},{"name":"淀粉","quantity":5,"unit":"克"}]',
     '1. 鸡丁腌制备用；2. 炸花生米；3. 爆香辣椒花椒；4. 炒鸡丁加料汁。',
     '[{"order":1,"description":"鸡胸肉切丁，加料酒、盐、淀粉抓匀腌制10分钟。","duration":600},{"order":2,"description":"花生米小火炒至金黄酥脆备用。","duration":180},{"order":3,"description":"碗中调酱汁：酱油、醋、糖、淀粉、清水搅匀备用。","duration":60},{"order":4,"description":"热锅凉油，小火炸香干辣椒和花椒，转大火下鸡丁翻炒至变色，加入葱蒜和酱汁翻炒均匀，最后撒花生米出锅。","duration":240}]',
     10, 10, 3, '{"川菜","麻辣","下饭"}', '炒', '麻辣', '荤', 'approved', 1, '{
    "calories": 380,
    "protein": 30,
    "fat": 18,
    "carbs": 22,
    "per_serving": {"calories": 380, "protein_g": 30, "fat_g": 18, "carbs_g": 22, "fiber_g": 2, "sodium_mg": 750, "vitamin_c_mg": 2},
    "ingredients_breakdown": [
        {"name": "鸡胸肉", "calories": 200, "protein_g": 25, "fat_g": 4, "carbs_g": 0},
        {"name": "花生米", "calories": 110, "protein_g": 5, "fat_g": 8, "carbs_g": 5},
        {"name": "干辣椒", "calories": 5, "protein_g": 0.3, "fat_g": 0.3, "carbs_g": 1}
    ],
    "health_notes": "含花生，过敏人群请注意。"
}'::jsonb),
    (7, '清蒸鲈鱼', '原汁原味，鲜嫩不腥',
     '[{"name":"鲈鱼","quantity":1,"unit":"条"},{"name":"葱","quantity":3,"unit":"根"},{"name":"姜","quantity":5,"unit":"片"},{"name":"蒸鱼豉油","quantity":20,"unit":"毫升"},{"name":"料酒","quantity":10,"unit":"毫升"}]',
     '1. 鱼身划刀塞姜片；2. 水开上锅蒸8分钟；3. 倒掉汤汁淋豉油泼热油。',
     '[{"order":1,"description":"鲈鱼去鳞去内脏洗净，鱼身两面各划三刀，塞入姜片，鱼身抹料酒。","duration":180},{"order":2,"description":"水烧开后放入鱼盘，大火蒸8分钟。","duration":480},{"order":3,"description":"倒掉盘中蒸出的汤汁，铺上葱丝姜丝，淋蒸鱼豉油，浇一勺滚烫热油激出香味。","duration":60}]',
     8, 10, 3, '{"清淡","海鲜","宴客"}', '蒸', '清淡', '海鲜', 'approved', 1, '{
    "calories": 180,
    "protein": 28,
    "fat": 6,
    "carbs": 2,
    "per_serving": {"calories": 180, "protein_g": 28, "fat_g": 6, "carbs_g": 2, "fiber_g": 0, "sodium_mg": 450, "vitamin_c_mg": 2},
    "ingredients_breakdown": [
        {"name": "鲈鱼", "calories": 160, "protein_g": 26, "fat_g": 5, "carbs_g": 0},
        {"name": "蒸鱼豉油", "calories": 10, "protein_g": 1, "fat_g": 0, "carbs_g": 2}
    ],
    "health_notes": "清淡低脂，富含优质蛋白，适合减脂期。"
}'::jsonb),
    (8, '酸辣土豆丝', '爽脆开胃，简单快手',
     '[{"name":"土豆","quantity":2,"unit":"个"},{"name":"干辣椒","quantity":3,"unit":"个"},{"name":"花椒","quantity":2,"unit":"克"},{"name":"白醋","quantity":10,"unit":"毫升"},{"name":"盐","quantity":3,"unit":"克"},{"name":"青椒","quantity":1,"unit":"个"}]',
     '1. 土豆切丝泡水；2. 爆香辣椒花椒；3. 大火快炒加醋。',
     '[{"order":1,"description":"土豆去皮切细丝，放入冷水中浸泡去除淀粉后沥干。","duration":180},{"order":2,"description":"青椒切丝备用。","duration":60},{"order":3,"description":"热锅放油，小火炸香干辣椒和花椒，转大火放入土豆丝翻炒至断生，加白醋、盐、青椒丝翻炒均匀出锅。","duration":180}]',
     8, 5, 2, '{"酸辣","快手","家常"}', '炒', '酸辣', '素', 'approved', 1, '{
    "calories": 160,
    "protein": 4,
    "fat": 3,
    "carbs": 30,
    "per_serving": {"calories": 160, "protein_g": 4, "fat_g": 3, "carbs_g": 30, "fiber_g": 3, "sodium_mg": 350, "vitamin_c_mg": 30},
    "ingredients_breakdown": [
        {"name": "土豆", "calories": 140, "protein_g": 3, "fat_g": 0.2, "carbs_g": 30},
        {"name": "干辣椒", "calories": 3, "protein_g": 0.2, "fat_g": 0.2, "carbs_g": 0.5}
    ],
    "health_notes": "醋溜做法低脂爽口，但淀粉含量较高。"
}'::jsonb),
    (9, '红烧肉', '肥而不腻，入口即化',
     '[{"name":"五花肉","quantity":500,"unit":"克"},{"name":"冰糖","quantity":20,"unit":"克"},{"name":"姜","quantity":4,"unit":"片"},{"name":"葱","quantity":2,"unit":"根"},{"name":"八角","quantity":2,"unit":"个"},{"name":"酱油","quantity":30,"unit":"毫升"},{"name":"老抽","quantity":10,"unit":"毫升"},{"name":"料酒","quantity":15,"unit":"毫升"}]',
     '1. 五花肉焯水切块；2. 炒糖色；3. 加料炖煮40分钟；4. 收汁出锅。',
     '[{"order":1,"description":"五花肉冷水下锅加姜片料酒焯水，煮出血沫后捞出切块。","duration":300},{"order":2,"description":"锅中放少许油，小火炒化冰糖至枣红色。","duration":180},{"order":3,"description":"放入五花肉翻炒上色，加入姜葱八角、酱油老抽翻炒均匀，加入开水没过肉面，大火烧开后转小火炖40分钟。","duration":2400},{"order":4,"description":"转大火收汁至浓稠挂汁，出锅装盘。","duration":180}]',
     15, 45, 4, '{"下饭","宴客","浓香"}', '炖', '酱香', '荤', 'approved', 1, '{
    "calories": 580,
    "protein": 28,
    "fat": 42,
    "carbs": 18,
    "per_serving": {"calories": 580, "protein_g": 28, "fat_g": 42, "carbs_g": 18, "fiber_g": 0, "sodium_mg": 800, "vitamin_c_mg": 0},
    "ingredients_breakdown": [
        {"name": "五花肉", "calories": 520, "protein_g": 24, "fat_g": 40, "carbs_g": 0},
        {"name": "冰糖", "calories": 60, "protein_g": 0, "fat_g": 0, "carbs_g": 16}
    ],
    "health_notes": "热量和脂肪含量较高，高血脂人群请适量食用。"
}'::jsonb),
    (10, '蒜蓉生菜', '脆嫩爽口，蒜香浓郁',
     '[{"name":"生菜","quantity":2,"unit":"颗"},{"name":"蒜","quantity":5,"unit":"瓣"},{"name":"蚝油","quantity":10,"unit":"毫升"},{"name":"盐","quantity":2,"unit":"克"}]',
     '1. 生菜焯水；2. 炒蒜蓉；3. 淋蚝油拌匀。',
     '[{"order":1,"description":"生菜洗净，沸水中加少许油和盐，烫10秒捞出摆盘。","duration":60},{"order":2,"description":"蒜切碎末，热锅放油小火炒至金黄出香。","duration":60},{"order":3,"description":"将蒜蓉和蚝油浇在生菜上，拌匀即可。","duration":30}]',
     5, 3, 2, '{"快手","素菜","低卡"}', '炒', '蒜香', '素', 'approved', 1, '{
    "calories": 60,
    "protein": 3,
    "fat": 2,
    "carbs": 8,
    "per_serving": {"calories": 60, "protein_g": 3, "fat_g": 2, "carbs_g": 8, "fiber_g": 2, "sodium_mg": 350, "vitamin_c_mg": 15},
    "ingredients_breakdown": [
        {"name": "生菜", "calories": 15, "protein_g": 1.5, "fat_g": 0.2, "carbs_g": 3},
        {"name": "蚝油", "calories": 15, "protein_g": 1, "fat_g": 0.5, "carbs_g": 3}
    ],
    "health_notes": "低卡快手，适合减脂期食用。"
}'::jsonb),
    (11, '回锅肉', '肥瘦相间，香辣下饭',
     '[{"name":"五花肉","quantity":300,"unit":"克"},{"name":"蒜苗","quantity":3,"unit":"根"},{"name":"豆瓣酱","quantity":15,"unit":"克"},{"name":"姜","quantity":3,"unit":"片"},{"name":"料酒","quantity":10,"unit":"毫升"},{"name":"糖","quantity":3,"unit":"克"}]',
     '1. 五花肉煮至断生切片；2. 煎至焦黄；3. 加豆瓣酱和蒜苗翻炒。',
     '[{"order":1,"description":"整块五花肉冷水下锅加姜片料酒，煮至筷子能插透捞出，放凉切薄片。","duration":1200},{"order":2,"description":"锅中不放油，放入肉片中火煎至两面微焦出油。","duration":180},{"order":3,"description":"加入豆瓣酱炒出红油，放入切段的蒜苗大火翻炒至断生，加少许糖提鲜出锅。","duration":120}]',
     10, 15, 3, '{"川菜","香辣","下饭"}', '炒', '香辣', '荤', 'approved', 1, '{
    "calories": 480,
    "protein": 22,
    "fat": 35,
    "carbs": 12,
    "per_serving": {"calories": 480, "protein_g": 22, "fat_g": 35, "carbs_g": 12, "fiber_g": 1, "sodium_mg": 850, "vitamin_c_mg": 5},
    "ingredients_breakdown": [
        {"name": "五花肉", "calories": 320, "protein_g": 15, "fat_g": 25, "carbs_g": 0},
        {"name": "豆瓣酱", "calories": 30, "protein_g": 2, "fat_g": 1, "carbs_g": 4}
    ],
    "health_notes": "高脂高钠，建议搭配清淡蔬菜食用。"
}'::jsonb),
    (12, '水煮鱼片', '鱼片嫩滑，麻辣鲜香',
     '[{"name":"草鱼","quantity":1,"unit":"条"},{"name":"豆芽","quantity":200,"unit":"克"},{"name":"干辣椒","quantity":10,"unit":"个"},{"name":"花椒","quantity":5,"unit":"克"},{"name":"姜","quantity":5,"unit":"片"},{"name":"蒜","quantity":5,"unit":"瓣"},{"name":"豆瓣酱","quantity":20,"unit":"克"},{"name":"蛋清","quantity":1,"unit":"个"},{"name":"淀粉","quantity":10,"unit":"克"}]',
     '1. 鱼片片好腌制；2. 炒底料煮汤；3. 烫豆芽铺底；4. 煮鱼片浇热油。',
     '[{"order":1,"description":"草鱼去骨片成薄片，加蛋清、淀粉、盐抓匀腌制15分钟。","duration":900},{"order":2,"description":"热锅放油炒香豆瓣酱、姜蒜末，加入足量清水烧开，熬煮5分钟成红汤。","duration":360},{"order":3,"description":"豆芽焯水后铺在大碗底部。","duration":120},{"order":4,"description":"红汤中逐片放入鱼片，煮至变白浮起即可捞出铺在豆芽上，撒干辣椒段、花椒和蒜末，浇一勺滚烫热油。","duration":180}]',
     20, 15, 3, '{"川菜","麻辣","宴客"}', '煮', '麻辣', '海鲜', 'approved', 1, '{
    "calories": 350,
    "protein": 32,
    "fat": 18,
    "carbs": 12,
    "per_serving": {"calories": 350, "protein_g": 32, "fat_g": 18, "carbs_g": 12, "fiber_g": 2, "sodium_mg": 950, "vitamin_c_mg": 5},
    "ingredients_breakdown": [
        {"name": "草鱼", "calories": 180, "protein_g": 28, "fat_g": 6, "carbs_g": 0},
        {"name": "豆芽", "calories": 15, "protein_g": 1.5, "fat_g": 0.2, "carbs_g": 3},
        {"name": "豆瓣酱", "calories": 40, "protein_g": 3, "fat_g": 2, "carbs_g": 5}
    ],
    "health_notes": "麻辣高钠，高血压患者请谨慎食用。"
}'::jsonb),
    (13, '蚝油牛肉', '滑嫩多汁，蚝香浓郁',
     '[{"name":"牛肉","quantity":250,"unit":"克"},{"name":"蚝油","quantity":20,"unit":"毫升"},{"name":"蒜","quantity":3,"unit":"瓣"},{"name":"姜","quantity":3,"unit":"片"},{"name":"青椒","quantity":1,"unit":"个"},{"name":"洋葱","quantity":0.5,"unit":"个"},{"name":"酱油","quantity":5,"unit":"毫升"},{"name":"淀粉","quantity":5,"unit":"克"}]',
     '1. 牛肉切薄片腌制；2. 滑炒牛肉盛出；3. 炒配菜加蚝油和牛肉回锅。',
     '[{"order":1,"description":"牛肉逆纹切薄片，加酱油、淀粉、油抓匀腌制10分钟。","duration":600},{"order":2,"description":"青椒切片，洋葱切丝备用。","duration":120},{"order":3,"description":"热锅多油，油温六成热下牛肉滑炒至变色立即盛出。","duration":120},{"order":4,"description":"锅中留底油爆香姜蒜，下青椒洋葱翻炒断生，倒入牛肉和蚝油快速翻炒均匀出锅。","duration":120}]',
     10, 5, 2, '{"快手","鲜香","下饭"}', '炒', '鲜香', '荤', 'approved', 1, '{
    "calories": 250,
    "protein": 26,
    "fat": 12,
    "carbs": 10,
    "per_serving": {"calories": 250, "protein_g": 26, "fat_g": 12, "carbs_g": 10, "fiber_g": 1, "sodium_mg": 500, "vitamin_c_mg": 20},
    "ingredients_breakdown": [
        {"name": "牛肉", "calories": 200, "protein_g": 22, "fat_g": 10, "carbs_g": 0},
        {"name": "蚝油", "calories": 15, "protein_g": 1, "fat_g": 0.5, "carbs_g": 3}
    ],
    "health_notes": "蚝油含钠，建议减少额外加盐。"
}'::jsonb),
    (14, '醋溜白菜', '酸甜脆爽，简单快手',
     '[{"name":"大白菜","quantity":300,"unit":"克"},{"name":"干辣椒","quantity":2,"unit":"个"},{"name":"醋","quantity":15,"unit":"毫升"},{"name":"糖","quantity":5,"unit":"克"},{"name":"盐","quantity":3,"unit":"克"},{"name":"淀粉","quantity":5,"unit":"克"}]',
     '1. 白菜切段；2. 爆香辣椒；3. 大火快炒调味勾芡。',
     '[{"order":1,"description":"大白菜洗净，菜帮斜切片，菜叶切段分开备用。","duration":120},{"order":2,"description":"碗中调汁：醋、糖、盐、淀粉、少许水搅匀。","duration":30},{"order":3,"description":"热锅放油，小火炸香干辣椒，转大火先下菜帮翻炒至微软，再加菜叶翻炒至断生，倒入调好的料汁快速翻炒均匀出锅。","duration":180}]',
     8, 5, 2, '{"酸甜","快手","家常"}', '炒', '酸甜', '素', 'approved', 1, '{
    "calories": 90,
    "protein": 3,
    "fat": 1,
    "carbs": 18,
    "per_serving": {"calories": 90, "protein_g": 3, "fat_g": 1, "carbs_g": 18, "fiber_g": 3, "sodium_mg": 400, "vitamin_c_mg": 25},
    "ingredients_breakdown": [
        {"name": "大白菜", "calories": 40, "protein_g": 2, "fat_g": 0.2, "carbs_g": 8},
        {"name": "干辣椒", "calories": 3, "protein_g": 0.2, "fat_g": 0.2, "carbs_g": 0.5}
    ],
    "health_notes": "低卡低脂，开胃爽口。"
}'::jsonb),
    (15, '干煸四季豆', '干香微辣，下饭神器',
     '[{"name":"四季豆","quantity":300,"unit":"克"},{"name":"干辣椒","quantity":4,"unit":"个"},{"name":"花椒","quantity":3,"unit":"克"},{"name":"蒜","quantity":3,"unit":"瓣"},{"name":"盐","quantity":3,"unit":"克"},{"name":"糖","quantity":2,"unit":"克"}]',
     '1. 四季豆去筋切段；2. 中小火煸至焦皱；3. 加辣椒蒜末调味。',
     '[{"order":1,"description":"四季豆去两角筋，掰成长段，沥干水分备用。","duration":120},{"order":2,"description":"锅中放油，小火煸炒四季豆至表面起焦皱、完全熟透，盛出备用。","duration":360},{"order":3,"description":"锅中留底油，下干辣椒段、花椒、蒜末小火煸香，放入煸好的四季豆，加盐、糖调味翻炒均匀出锅。","duration":120}]',
     10, 10, 3, '{"干香","下饭","素菜"}', '煸', '干香', '素', 'approved', 1, '{
    "calories": 180,
    "protein": 8,
    "fat": 10,
    "carbs": 18,
    "per_serving": {"calories": 180, "protein_g": 8, "fat_g": 10, "carbs_g": 18, "fiber_g": 5, "sodium_mg": 300, "vitamin_c_mg": 10},
    "ingredients_breakdown": [
        {"name": "四季豆", "calories": 130, "protein_g": 6, "fat_g": 0.5, "carbs_g": 16},
        {"name": "干辣椒", "calories": 3, "protein_g": 0.2, "fat_g": 0.2, "carbs_g": 0.5}
    ],
    "health_notes": "四季豆必须完全熟透，防止食物中毒。"
}'::jsonb),
    (16, '葱爆羊肉', '羊肉嫩滑，葱香四溢',
     '[{"name":"羊肉","quantity":300,"unit":"克"},{"name":"大葱","quantity":2,"unit":"根"},{"name":"姜","quantity":3,"unit":"片"},{"name":"料酒","quantity":10,"unit":"毫升"},{"name":"酱油","quantity":10,"unit":"毫升"},{"name":"孜然","quantity":3,"unit":"克"},{"name":"盐","quantity":2,"unit":"克"}]',
     '1. 羊肉切薄片腌制；2. 大葱切滚刀块；3. 大火爆炒。',
     '[{"order":1,"description":"羊肉切薄片，加料酒、酱油、盐抓匀腌制5分钟。","duration":300},{"order":2,"description":"大葱切成滚刀块，姜切丝备用。","duration":120},{"order":3,"description":"大火热锅多油，油冒烟时下羊肉快速滑炒至变色立刻盛出。","duration":60},{"order":4,"description":"锅中留底油爆香姜丝和大葱块，倒入羊肉，撒孜然粉大火翻炒几下立即出锅。","duration":60}]',
     8, 5, 2, '{"快手","葱香","下饭"}', '爆', '葱香', '荤', 'approved', 1, '{
    "calories": 320,
    "protein": 24,
    "fat": 20,
    "carbs": 8,
    "per_serving": {"calories": 320, "protein_g": 24, "fat_g": 20, "carbs_g": 8, "fiber_g": 1, "sodium_mg": 500, "vitamin_c_mg": 3},
    "ingredients_breakdown": [
        {"name": "羊肉", "calories": 280, "protein_g": 22, "fat_g": 18, "carbs_g": 0},
        {"name": "大葱", "calories": 15, "protein_g": 0.5, "fat_g": 0.2, "carbs_g": 4}
    ],
    "health_notes": "羊肉性温，适合冬季食用。"
}'::jsonb),
    (17, '紫菜蛋花汤', '简单快手，鲜美暖胃',
     '[{"name":"紫菜","quantity":5,"unit":"克"},{"name":"鸡蛋","quantity":2,"unit":"个"},{"name":"葱","quantity":1,"unit":"根"},{"name":"盐","quantity":2,"unit":"克"},{"name":"香油","quantity":2,"unit":"毫升"}]',
     '1. 紫菜泡发撕碎；2. 水开淋蛋液；3. 加紫菜调味。',
     '[{"order":1,"description":"紫菜用温水泡开，撕碎备用。","duration":60},{"order":2,"description":"鸡蛋打散备用，葱切葱花。","duration":60},{"order":3,"description":"锅中水烧开，沿筷子缓缓倒入蛋液形成蛋花，放入紫菜，加盐调味，出锅前淋香油撒葱花。","duration":120}]',
     3, 5, 2, '{"清淡","快手","汤羹"}', '煮', '清淡', '素', 'approved', 1, '{
    "calories": 80,
    "protein": 8,
    "fat": 3,
    "carbs": 5,
    "per_serving": {"calories": 80, "protein_g": 8, "fat_g": 3, "carbs_g": 5, "fiber_g": 0, "sodium_mg": 350, "vitamin_c_mg": 2},
    "ingredients_breakdown": [
        {"name": "紫菜", "calories": 10, "protein_g": 1.5, "fat_g": 0.1, "carbs_g": 1},
        {"name": "鸡蛋", "calories": 70, "protein_g": 6, "fat_g": 5, "carbs_g": 0}
    ],
    "health_notes": "清淡暖胃，适合饭后饮用。"
}'::jsonb),
    (18, '鱼香肉丝', '酸甜带辣，经典川菜',
     '[{"name":"猪里脊","quantity":250,"unit":"克"},{"name":"木耳","quantity":50,"unit":"克"},{"name":"胡萝卜","quantity":1,"unit":"根"},{"name":"豆瓣酱","quantity":10,"unit":"克"},{"name":"醋","quantity":10,"unit":"毫升"},{"name":"糖","quantity":10,"unit":"克"},{"name":"酱油","quantity":5,"unit":"毫升"},{"name":"淀粉","quantity":5,"unit":"克"},{"name":"姜","quantity":3,"unit":"片"},{"name":"蒜","quantity":3,"unit":"瓣"}]',
     '1. 肉丝腌制滑炒；2. 调鱼香汁；3. 炒配菜加肉丝和料汁。',
     '[{"order":1,"description":"猪里脊切丝，加盐、淀粉、料酒抓匀腌制10分钟。","duration":600},{"order":2,"description":"木耳泡发切丝，胡萝卜切丝备用。","duration":180},{"order":3,"description":"碗中调鱼香汁：醋、糖、酱油、淀粉、清水搅匀。","duration":30},{"order":4,"description":"热锅放油将肉丝滑炒至变色盛出，锅中留底油炒香豆瓣酱、姜蒜末，下木耳丝胡萝卜丝翻炒，倒入肉丝和鱼香汁大火翻炒均匀出锅。","duration":240}]',
     12, 8, 3, '{"川菜","酸甜","下饭"}', '炒', '酸甜', '荤', 'approved', 1, '{
    "calories": 350,
    "protein": 24,
    "fat": 12,
    "carbs": 35,
    "per_serving": {"calories": 350, "protein_g": 24, "fat_g": 12, "carbs_g": 35, "fiber_g": 3, "sodium_mg": 700, "vitamin_c_mg": 5},
    "ingredients_breakdown": [
        {"name": "猪里脊", "calories": 200, "protein_g": 18, "fat_g": 6, "carbs_g": 0},
        {"name": "木耳", "calories": 15, "protein_g": 0.5, "fat_g": 0.1, "carbs_g": 4},
        {"name": "胡萝卜", "calories": 25, "protein_g": 0.5, "fat_g": 0.2, "carbs_g": 6}
    ],
    "health_notes": "鱼香口味含糖量较高，糖尿病患者留意。"
}'::jsonb),
    (19, '西红柿炖牛腩', '汤汁浓郁，肉烂入味',
     '[{"name":"牛腩","quantity":500,"unit":"克"},{"name":"番茄","quantity":3,"unit":"个"},{"name":"洋葱","quantity":0.5,"unit":"个"},{"name":"姜","quantity":4,"unit":"片"},{"name":"番茄酱","quantity":20,"unit":"克"},{"name":"料酒","quantity":10,"unit":"毫升"},{"name":"盐","quantity":5,"unit":"克"}]',
     '1. 牛腩焯水切块；2. 炒番茄出汁；3. 加入牛腩炖煮1小时。',
     '[{"order":1,"description":"牛腩切块冷水下锅加姜片料酒焯水，煮出血沫后捞出洗净。","duration":300},{"order":2,"description":"番茄顶部划十字烫一下去皮切块，洋葱切块。","duration":180},{"order":3,"description":"热锅放油炒香洋葱，下番茄块炒出汁水，加入番茄酱翻炒均匀。","duration":180},{"order":4,"description":"放入牛腩块，加足量热水没过食材，大火烧开后转小火炖1小时至牛腩软烂，加盐调味出锅。","duration":3600}]',
     15, 60, 4, '{"浓郁","下饭","宴客"}', '炖', '浓郁', '荤', 'approved', 1, '{
    "calories": 420,
    "protein": 38,
    "fat": 22,
    "carbs": 16,
    "per_serving": {"calories": 420, "protein_g": 38, "fat_g": 22, "carbs_g": 16, "fiber_g": 2, "sodium_mg": 650, "vitamin_c_mg": 20},
    "ingredients_breakdown": [
        {"name": "牛腩", "calories": 300, "protein_g": 30, "fat_g": 18, "carbs_g": 0},
        {"name": "番茄", "calories": 40, "protein_g": 2, "fat_g": 0.5, "carbs_g": 8},
        {"name": "洋葱", "calories": 15, "protein_g": 0.5, "fat_g": 0.1, "carbs_g": 4}
    ],
    "health_notes": "番茄红素丰富，牛肉补充铁质。"
}'::jsonb),
    (20, '蒜蓉粉丝蒸虾', '粉丝吸汁，虾肉鲜甜',
     '[{"name":"大虾","quantity":10,"unit":"只"},{"name":"粉丝","quantity":50,"unit":"克"},{"name":"蒜","quantity":6,"unit":"瓣"},{"name":"葱","quantity":2,"unit":"根"},{"name":"生抽","quantity":10,"unit":"毫升"},{"name":"料酒","quantity":5,"unit":"毫升"}]',
     '1. 粉丝泡软铺底；2. 处理虾开背；3. 蒜蓉炒香铺在虾上；4. 上锅蒸5分钟。',
     '[{"order":1,"description":"粉丝用温水泡软铺在盘底。","duration":180},{"order":2,"description":"大虾去须开背去虾线，用料酒腌制5分钟，整齐摆在粉丝上。","duration":360},{"order":3,"description":"蒜切碎末，热锅放油小火炒至微黄出香，加生抽搅匀后铺在虾上。","duration":120},{"order":4,"description":"水烧开后放入虾盘，大火蒸5分钟，出锅撒葱花。","duration":300}]',
     15, 8, 3, '{"蒜香","海鲜","宴客"}', '蒸', '蒜香', '海鲜', 'approved', 1, '{
    "calories": 220,
    "protein": 24,
    "fat": 8,
    "carbs": 14,
    "per_serving": {"calories": 220, "protein_g": 24, "fat_g": 8, "carbs_g": 14, "fiber_g": 0, "sodium_mg": 550, "vitamin_c_mg": 2},
    "ingredients_breakdown": [
        {"name": "大虾", "calories": 100, "protein_g": 20, "fat_g": 1.5, "carbs_g": 0},
        {"name": "粉丝", "calories": 80, "protein_g": 0.5, "fat_g": 0, "carbs_g": 18}
    ],
    "health_notes": "虾肉高蛋白低脂肪，蒸制做法健康清淡。"
}'::jsonb),
    (21, '地三鲜', '东北名菜，鲜香四溢',
     '[{"name":"土豆","quantity":1,"unit":"个"},{"name":"茄子","quantity":1,"unit":"个"},{"name":"青椒","quantity":2,"unit":"个"},{"name":"蒜","quantity":3,"unit":"瓣"},{"name":"酱油","quantity":10,"unit":"毫升"},{"name":"糖","quantity":3,"unit":"克"},{"name":"盐","quantity":2,"unit":"克"},{"name":"淀粉","quantity":5,"unit":"克"}]',
     '1. 土豆茄子分别煎炸；2. 锅中炒香蒜末；3. 加回所有食材调味。',
     '[{"order":1,"description":"土豆去皮切滚刀块，茄子切滚刀块，青椒切片。","duration":180},{"order":2,"description":"土豆块入油锅煎至金黄微焦盛出，茄子块煎至表面焦软盛出。","duration":360},{"order":3,"description":"锅中留底油爆香蒜末，放入青椒片翻炒，倒入土豆茄子，加酱油、糖、盐和少许水翻炒均匀，水淀粉勾薄芡出锅。","duration":180}]',
     10, 12, 3, '{"东北菜","家常","素菜"}', '炒', '鲜香', '素', 'approved', 1, '{
    "calories": 200,
    "protein": 5,
    "fat": 8,
    "carbs": 30,
    "per_serving": {"calories": 200, "protein_g": 5, "fat_g": 8, "carbs_g": 30, "fiber_g": 4, "sodium_mg": 400, "vitamin_c_mg": 35},
    "ingredients_breakdown": [
        {"name": "土豆", "calories": 80, "protein_g": 2, "fat_g": 0.1, "carbs_g": 18},
        {"name": "茄子", "calories": 40, "protein_g": 1, "fat_g": 0.2, "carbs_g": 8},
        {"name": "青椒", "calories": 10, "protein_g": 0.5, "fat_g": 0.1, "carbs_g": 2}
    ],
    "health_notes": "蔬菜丰富，营养素均衡。"
}'::jsonb),
    (22, '口水鸡', '鸡肉嫩滑，麻辣鲜香',
     '[{"name":"鸡腿","quantity":2,"unit":"只"},{"name":"姜","quantity":3,"unit":"片"},{"name":"葱","quantity":2,"unit":"根"},{"name":"花椒","quantity":2,"unit":"克"},{"name":"辣椒油","quantity":15,"unit":"毫升"},{"name":"酱油","quantity":10,"unit":"毫升"},{"name":"醋","quantity":5,"unit":"毫升"},{"name":"糖","quantity":5,"unit":"克"},{"name":"花生碎","quantity":10,"unit":"克"},{"name":"蒜","quantity":3,"unit":"瓣"}]',
     '1. 鸡腿煮熟冰镇；2. 斩块摆盘；3. 淋红油酱汁撒花生碎。',
     '[{"order":1,"description":"鸡腿冷水下锅加姜片葱段和花椒，煮开后转中火煮15分钟至熟透，捞出放入冰水中浸泡5分钟。","duration":1200},{"order":2,"description":"冰镇后的鸡腿斩成小块，摆入盘中。","duration":180},{"order":3,"description":"碗中调汁：辣椒油、酱油、醋、糖、蒜末搅匀，淋在鸡块上，撒花生碎和葱花即可。","duration":60}]',
     10, 20, 2, '{"川菜","麻辣","凉菜"}', '拌', '麻辣', '荤', 'approved', 1, '{
    "calories": 380,
    "protein": 26,
    "fat": 24,
    "carbs": 12,
    "per_serving": {"calories": 380, "protein_g": 26, "fat_g": 24, "carbs_g": 12, "fiber_g": 1, "sodium_mg": 780, "vitamin_c_mg": 2},
    "ingredients_breakdown": [
        {"name": "鸡腿", "calories": 250, "protein_g": 24, "fat_g": 15, "carbs_g": 0},
        {"name": "辣椒油", "calories": 60, "protein_g": 0, "fat_g": 7, "carbs_g": 0},
        {"name": "花生碎", "calories": 25, "protein_g": 2, "fat_g": 2, "carbs_g": 1}
    ],
    "health_notes": "麻辣鲜香，含花生过敏原。"
}'::jsonb),
    (23, '蛋炒饭', '粒粒分明，简单管饱',
     '[{"name":"米饭","quantity":300,"unit":"克"},{"name":"鸡蛋","quantity":2,"unit":"个"},{"name":"火腿","quantity":50,"unit":"克"},{"name":"葱","quantity":2,"unit":"根"},{"name":"盐","quantity":3,"unit":"克"},{"name":"油","quantity":15,"unit":"毫升"}]',
     '1. 鸡蛋炒散盛出；2. 火腿爆香；3. 加米饭和鸡蛋翻炒。',
     '[{"order":1,"description":"鸡蛋打散，火腿切丁，葱切葱花备用。","duration":120},{"order":2,"description":"热锅多油，倒入蛋液快速炒散成蛋碎盛出。","duration":60},{"order":3,"description":"锅中加少许油火腿丁煸香，倒入米饭大火翻炒至粒粒分明，加入鸡蛋碎和盐翻炒均匀，撒葱花出锅。","duration":240}]',
     5, 5, 2, '{"快手","管饱","家常"}', '炒', '鲜香', '荤', 'approved', 1, '{
    "calories": 420,
    "protein": 14,
    "fat": 12,
    "carbs": 60,
    "per_serving": {"calories": 420, "protein_g": 14, "fat_g": 12, "carbs_g": 60, "fiber_g": 0, "sodium_mg": 500, "vitamin_c_mg": 1},
    "ingredients_breakdown": [
        {"name": "米饭", "calories": 350, "protein_g": 7, "fat_g": 1, "carbs_g": 75},
        {"name": "鸡蛋", "calories": 70, "protein_g": 6, "fat_g": 5, "carbs_g": 0}
    ],
    "health_notes": "碳水化合物含量较高，适合运动后补充能量。"
}'::jsonb)
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
    nutrition_info = EXCLUDED.nutrition_info,
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