// HealthConditionLists.h —— 健康忌口名单唯一事实源（整改 N2）
//
// 背景（2026-09-05）：名单曾手抄于 推荐引擎（RecipeServiceImpl 的 CONDITION_HARD_EXCLUDED /
// CONDITION_SOFT_ADVISED / 软档提示模板）、健康档案忌口建议文案（UserServiceImpl）、
// api-spec 4.2 三处，任一处漂移都会造成"文案承诺 ≠ 引擎行为"。
//
// 本头文件为服务端唯一名单来源：
//   · 推荐引擎（RecipeServiceImpl）的硬档剔除、软档提示、提示文案模板直接引用这里；
//   · UserService 忌口建议的食材名单由这里 join 生成（reason 文案仍属 UI 文案，本地维护）；
//   · api-spec 4.2 为面向客户端的文档快照（REST 契约无法引用 C++ 头文件），
//     改名单时需同步 spec 修订记录并留意其 4.2 名单段落。
//
// 约定：假设数据库健康条件使用中文 locale；名单顺序 = 文案 join 顺序，勿随意调整。
// 档位语义：
//   硬档（HARD）：食材出现即整道菜剔除——高盐/高糖加工品或公认高风险食材，剔除有明确依据；
//   软档（SOFT）：常见调味料/调味糖源——做菜时可少放或不放，不剔除，仅提示 + 评分轻微降权。
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

namespace gocook::health {

/// 硬档名单：健康条件 → 出现即整道排除的食材
inline const std::unordered_map<std::string, std::vector<std::string>>& hardExclusions() {
    static const std::unordered_map<std::string, std::vector<std::string>> kMap = {
        {"高血压", {"咸菜", "腊肉", "咸鱼", "腐乳", "榨菜"}},
        {"糖尿病", {"甜面酱", "炼乳", "果酱"}},
        {"高血脂", {"肥肉", "猪油", "黄油", "奶油", "五花肉", "油炸", "猪板油"}},
        {"痛风",   {"海鲜", "动物内脏", "啤酒", "浓汤", "香菇", "虾", "蟹"}},
    };
    return kMap;
}

/// 软档名单：健康条件 → 仅提示 + 轻微降权（不剔除）的调味料/调味糖源
inline const std::unordered_map<std::string, std::vector<std::string>>& softAdvisories() {
    static const std::unordered_map<std::string, std::vector<std::string>> kMap = {
        {"高血压", {"盐", "酱油", "豆瓣酱"}},
        {"糖尿病", {"糖", "白糖", "冰糖", "蜂蜜"}},
    };
    return kMap;
}

/// 软档提示文案模板：%s 处填充该条件命中的食材（join("、")）。
/// 数组序 = 多条件命中时文案的拼接序（高血压 → 糖尿病），勿调换。
inline const std::vector<std::pair<std::string, std::string>>& softNoticeTemplates() {
    static const std::vector<std::pair<std::string, std::string>> kTemplates = {
        {"高血压", "含%s，高血压人群建议少盐清淡"},
        {"糖尿病", "含%s，糖尿病人群可选择不放或少放"},
    };
    return kTemplates;
}

/// 按条件取硬档名单；条件不在表中返回空
inline const std::vector<std::string>& hardExcludedFor(const std::string& condition) {
    static const std::vector<std::string> kEmpty;
    const auto& map = hardExclusions();
    auto it = map.find(condition);
    return it != map.end() ? it->second : kEmpty;
}

/// 按条件取软档名单；条件不在表中返回空
inline const std::vector<std::string>& softAdvisedFor(const std::string& condition) {
    static const std::vector<std::string> kEmpty;
    const auto& map = softAdvisories();
    auto it = map.find(condition);
    return it != map.end() ? it->second : kEmpty;
}

}  // namespace gocook::health
