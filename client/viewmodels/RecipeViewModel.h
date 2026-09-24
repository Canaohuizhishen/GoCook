#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QSet>
#include <memory>
#include <gocook/IGoCookApi.h>
#include "RequestGuards.h"

/**
 * @brief 菜谱域 ViewModel：QML 侧唯一的菜谱数据入口（setContextProperty("recipeVM") 注入，main.cpp:60）。
 *
 * 所有 Q_INVOKABLE 异步：立即返回，结果经 Q_PROPERTY + NOTIFY 驱动 QML 绑定刷新；
 * 底层统一调用 IGoCookApi（HttpGoCookApi 经 HTTP 实现）。
 *
 * 三条贯穿全类的设计（具体契约见各成员注释）：
 *   1. 过期响应作废按语义三分（RequestGuards.h）：详情族五个请求（detail / nutrition / videos /
 *      ratings / myRating）共用 m_detailRequestedId——“目标键”比对（看的还是这道菜吗）；
 *      搜索用 m_searchEpoch（RequestEpoch）——“轮次”作废（换词后旧词响应丢弃）；
 *      收藏用 SessionSnapshot——“会话”作废（登出/换号后旧账号响应丢弃）。
 *   2. 乐观分档：deleteRecipe / deleteRating / deleteFavoriteGroup 由 VM 乐观并回滚；
 *      toggleFavorite 由 QML 乐观（失败自行回滚）；其余写操作等服务器确认后发结果信号。
 *   3. 错误按域分通道（searchErrorOccurred / ratingError / favoriteOperationFailed /
 *      专用 *Failed，另有静默项）——勿只监听 errorOccurred，详见 signals 区。
 */
class RecipeViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList recipes READ recipes NOTIFY recipesChanged)   ///< 列表数据（公开/推荐共用；推荐模式每次整体替换）
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)   ///< 是否还有下一页（仅公开模式会为 true，推荐模式恒 false）
    Q_PROPERTY(bool healthFilterApplied READ healthFilterApplied NOTIFY healthFilterAppliedChanged)   ///< 推荐结果是否已应用健康过滤（仅推荐模式更新）
    Q_PROPERTY(QVariantMap recipeDetail READ recipeDetail NOTIFY recipeDetailChanged)   ///< 详情与编辑表单共用（进入对应流程先清空）
    Q_PROPERTY(bool detailLoading READ detailLoading NOTIFY detailLoadingChanged)   ///< 详情/编辑取数共用
    Q_PROPERTY(bool detailLoadFailed READ detailLoadFailed NOTIFY detailLoadFailedChanged)   ///< 详情失败且无缓存快照（页面居中离线视图）
    Q_PROPERTY(bool favoritesLoadFailed READ favoritesLoadFailed NOTIFY favoritesLoadFailedChanged)   ///< 收藏列表加载失败（无文案；未登录时数据已清空）
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(bool searchLoading READ searchLoading NOTIFY searchLoadingChanged)
    Q_PROPERTY(bool searchHasMore READ searchHasMore NOTIFY searchHasMoreChanged)
    Q_PROPERTY(bool searchPerformed READ searchPerformed NOTIFY searchPerformedChanged)   ///< 已执行过搜索：区分"无结果"与"尚未搜索"两种空态
    Q_PROPERTY(QVariantMap nutritionReport READ nutritionReport NOTIFY nutritionReportChanged)   ///< 营养报告（仅成功时更新）
    Q_PROPERTY(bool nutritionLoading READ nutritionLoading NOTIFY nutritionLoadingChanged)
    Q_PROPERTY(QVariantList recipeVideos READ recipeVideos NOTIFY recipeVideosChanged)   ///< 视频列表（失败静默；进入详情时先清空）
    Q_PROPERTY(bool videosLoading READ videosLoading NOTIFY videosLoadingChanged)
    Q_PROPERTY(QVariantList recipeRatings READ recipeRatings NOTIFY recipeRatingsChanged)
    Q_PROPERTY(bool ratingsLoading READ ratingsLoading NOTIFY ratingsLoadingChanged)
    Q_PROPERTY(bool ratingsHasMore READ ratingsHasMore NOTIFY ratingsHasMoreChanged)
    Q_PROPERTY(QVariantMap myRating READ myRating NOTIFY myRatingChanged)   ///< 我的评分（空 map=未评分）
    Q_PROPERTY(QVariantList myRecipes READ myRecipes NOTIFY myRecipesChanged)
    Q_PROPERTY(bool myRecipesLoading READ myRecipesLoading NOTIFY myRecipesLoadingChanged)
    Q_PROPERTY(bool myRecipesHasMore READ myRecipesHasMore NOTIFY myRecipesHasMoreChanged)
    Q_PROPERTY(QVariantList myRatings READ myRatings NOTIFY myRatingsChanged)
    Q_PROPERTY(bool myRatingsLoading READ myRatingsLoading NOTIFY myRatingsLoadingChanged)
    Q_PROPERTY(bool myRatingsHasMore READ myRatingsHasMore NOTIFY myRatingsHasMoreChanged)
    Q_PROPERTY(QVariantList favorites READ favorites NOTIFY favoritesChanged)   ///< 收藏列表（按当前分组筛选，""=全部）
    Q_PROPERTY(int favoritesAllCount READ favoritesAllCount NOTIFY favoriteGroupsChanged)   ///< 「全部」总数=Σ 分组 count（含合成默认收藏夹），只随分组数据变化，不受列表筛选覆盖
    Q_PROPERTY(bool favoritesHasMore READ favoritesHasMore NOTIFY favoritesHasMoreChanged)
    Q_PROPERTY(bool favoritesLoading READ favoritesLoading NOTIFY favoritesLoadingChanged)
    Q_PROPERTY(QVariantList favoriteGroups READ favoriteGroups NOTIFY favoriteGroupsChanged)   ///< 收藏分组（含服务端合成的默认组）
    Q_PROPERTY(QString apiBaseUrl READ apiBaseUrl CONSTANT)   ///< CONSTANT：服务端地址（自 httpApi），QML 用于拼接资源 URL

public:
    explicit RecipeViewModel(IGoCookApi *api, QObject *parent = nullptr);   ///< api：API 门面（生产 HttpGoCookApi；测试注入桩）

    // getter 组：返回同名 Q_PROPERTY 的值（语义见属性行）；仅特殊实现单独注明。
    QString apiBaseUrl() const;   ///< 特殊实现：dynamic_cast 到 HttpGoCookApi 取服务端地址；非 HTTP 实现回退 http://127.0.0.1:8080
    QVariantList recipes() const;
    bool isLoading() const;
    bool hasMore() const;
    bool healthFilterApplied() const;
    QVariantMap recipeDetail() const;
    bool detailLoading() const;
    bool detailLoadFailed() const;
    bool favoritesLoadFailed() const;
    QVariantList searchResults() const;
    bool searchLoading() const;
    bool searchHasMore() const;
    bool searchPerformed() const;
    QVariantMap nutritionReport() const;
    bool nutritionLoading() const;
    QVariantList recipeVideos() const;
    bool videosLoading() const;
    QVariantList recipeRatings() const;
    bool ratingsLoading() const;
    bool ratingsHasMore() const;
    QVariantMap myRating() const;
    QVariantList myRecipes() const;
    bool myRecipesLoading() const;
    bool myRecipesHasMore() const;
    QVariantList myRatings() const;
    bool myRatingsLoading() const;
    bool myRatingsHasMore() const;
    QVariantList favorites() const { return m_favorites; }
    int favoritesAllCount() const;   ///< 派生计算：Σ favoriteGroups[*].count（cpp 实现）
    bool favoritesHasMore() const { return m_favoritesHasMore; }
    bool favoritesLoading() const { return m_favoritesLoading; }
    QVariantList favoriteGroups() const { return m_favoriteGroups; }

    // ===== 分页大小常量（单一来源：默认参数 / 内部续页 / 成员初始化共用；调用点勿再传字面量） =====
    static constexpr int kPageSize = 20;         ///< 搜索 / 收藏 / 我的投稿 / 我的评论
    static constexpr int kPublicPageSize = 30;   ///< 公开列表 / 推荐列表（续页固定按此值，首屏勿传其他值）
    static constexpr int kRatingPageSize = 10;   ///< 详情页评分列表

    // ===== 菜谱列表与详情 =====
    /// 加载公开列表（page=1 替换，否则追加；size 仅本次生效，续页固定按 kPublicPageSize）。失败发 errorOccurred。
    Q_INVOKABLE void loadPublicRecipes(int page = 1, int size = kPublicPageSize);
    /// 加载推荐列表（整体替换，无续页）。失败发 errorOccurred。
    Q_INVOKABLE void loadRecommendedRecipes(int page = 1, int size = kPublicPageSize);
    Q_INVOKABLE void loadNextPage();   ///< 加载公开列表下一页（推荐模式调用为空操作）
    Q_INVOKABLE void refresh();   ///< 重新加载当前模式的第一页
    /// 进入即清空并重载（切菜谱不短暂显示旧数据）；失败：有快照静默显示、无快照置
    /// detailLoadFailed（不发 errorOccurred）；期间切走则本响应作废（m_detailRequestedId）。
    Q_INVOKABLE void loadRecipeDetail(int recipeId);
    // ===== 投稿与编辑 =====
    /// 提交投稿。ingredients: [{name, quantity, unit}]；steps: [{order, description, duration?}]。
    /// 成功发 recipeSubmitted(id, status)，失败发 submitFailed。
    Q_INVOKABLE void submitRecipe(const QString& name, const QString& description,
                                   const QString& imageUrl, const QVariantList& ingredients,
                                   const QVariantList& steps, const QVariantList& tags);
    /// 上传封面图。结果发 recipeImageUploaded(imageUrl) / recipeImageUploadFailed。
    Q_INVOKABLE void uploadRecipeImage(int recipeId, const QString& filePath);
    /// 上传步骤图（stepIndex 原样带回）。结果发 stepImageUploaded / stepImageUploadFailed。
    Q_INVOKABLE void uploadStepImage(int recipeId, int stepIndex, const QString& filePath);
    /// 保存编辑（参数同 submitRecipe；步骤额外接受 image_url）。成功发 recipeEdited，失败发 editFailed。
    Q_INVOKABLE void editRecipe(int recipeId, const QString& name, const QString& description,
                                 const QString& imageUrl, const QVariantList& ingredients,
                                 const QVariantList& steps, const QVariantList& tags);
    /// 编辑页取数：进入先清空 recipeDetail 再重载（与详情流程共用该属性）；
    /// 成功发 editFormDataReady，失败发 errorOccurred。
    Q_INVOKABLE void loadRecipeForEdit(int recipeId);
    // ===== 搜索 =====
    /// 搜索（keyword 空白则忽略；page=1 重置空态并替换结果）。失败发 searchErrorOccurred。
    Q_INVOKABLE void searchRecipes(const QString& keyword, int page = 1, int size = kPageSize);
    /// 搜索续页（沿用首屏 keyword 与 size）。
    Q_INVOKABLE void searchNextPage();
    Q_INVOKABLE void resetSearch();   ///< 清空搜索结果与空态标记（返回/清空输入时调用）
    // ===== 营养 / 视频 / 评分评论 =====
    /// 加载营养报告。成功发 nutritionReportChanged；失败发 errorOccurred；期间切走则响应作废。
    Q_INVOKABLE void loadNutritionReport(int recipeId);
    /// 加载视频（失败静默；期间切走则响应作废）。
    Q_INVOKABLE void loadRecipeVideos(int recipeId);
    /// 加载评分列表（page=1 先清空；失败静默）。续页用 loadMoreRatings。
    Q_INVOKABLE void loadRecipeRatings(int recipeId, int page = 1, int size = kRatingPageSize);
    /// 评分续页（固定按 kRatingPageSize）。
    Q_INVOKABLE void loadMoreRatings();
    /// 查询我的评分（未评分/失败静默清空，不发错误信号）。
    Q_INVOKABLE void loadMyRecipeRating(int recipeId);
    /// 发表评分。成功发 ratingSubmitted，失败发 ratingError。
    Q_INVOKABLE void rateRecipe(int recipeId, int rating, const QString& comment);
    /// 修改评分。成功发 ratingUpdated(ratingId)，失败发 ratingError。
    Q_INVOKABLE void updateRating(int recipeId, int ratingId, int rating, const QString& comment);
    /// 删除评分（VM 乐观移除并自动回滚；失败发 ratingError，成功发 ratingDeleted）。
    Q_INVOKABLE void deleteRating(int recipeId, int ratingId);
    Q_INVOKABLE void deleteRecipe(int recipeId);   ///< 删除我的投稿（发起即乐观移除；成功后重拉，失败回滚并发 deleteFailed）
    // ===== 我的投稿与评论 =====
    Q_INVOKABLE void loadMyRecipes(int page = 1, const QString& status = "", int size = kPageSize);   ///< 加载中重入忽略；status: "pending"/"approved"/"rejected"，空串=全部
    /// 投稿续页（固定按 kPageSize，沿用当前 status）。
    Q_INVOKABLE void loadMyRecipesNextPage();
    /// 我的评论（加载中重入忽略）。
    Q_INVOKABLE void loadMyRatings(int page = 1, int size = kPageSize);
    /// 评论续页（固定按 kPageSize）。
    Q_INVOKABLE void loadMyRatingsNextPage();

    // ===== 收藏 =====
    /// 加载收藏列表（group 空=全部；page=1 替换，否则追加；加载中重入忽略）。
    /// 发送时快照会话：响应回来若已登出/换号静默丢弃；失败置 favoritesLoadFailed（未登录先清空旧数据）。
    Q_INVOKABLE void loadFavorites(int page = 1, const QString &group = "", int size = kPageSize);
    /// 收藏续页（沿用首屏分组与 size）。
    Q_INVOKABLE void loadMoreFavorites();
    /// 清空收藏列表与分组、加载状态（登出/账号切换时调用，杜绝上一账号残留数据串台）
    Q_INVOKABLE void clearFavorites();
    /// 清空我的投稿与我的评论列表、分页与在途状态（登出/账号切换时调用；main.cpp sessionEnded 单点接线消费）——
    /// 这两页对游客可见，残留会让上一账号的投稿/评论直接呈现
    Q_INVOKABLE void clearMyContent();
    /// 收藏/取消收藏（groupId>0 收藏到该分组，0=默认夹）。成功发 favoriteToggleSuccess，
    /// 失败发 favoriteOperationFailed——页面自行维护乐观状态并回滚。
    Q_INVOKABLE void toggleFavorite(int recipeId, int groupId = 0);
    /// 拉取收藏分组（失败时主加载在途或已失败则静默）。
    Q_INVOKABLE void loadFavoriteGroups();
    /// 新建分组（成功重拉分组并发 favoriteGroupCreated；失败 favoriteOperationFailed）。
    Q_INVOKABLE void createFavoriteGroup(const QString &name);
    /// 删除分组（乐观移除并回滚；默认组拒删）。成功发 favoriteGroupDeleted。
    Q_INVOKABLE void deleteFavoriteGroup(int groupId);
    /// 取消收藏（等服务器确认；成功发 favoriteRemoved）。
    Q_INVOKABLE void removeFavorite(int favoriteId);
    Q_INVOKABLE void batchRemoveFavorites(const QVariantList &favoriteIds);   ///< 批量删除，完成后发 favoriteRemoved
    /// 移动收藏到指定分组（单条=N=1 复用批量更新端点；成功发 favoriteMoved）。
    Q_INVOKABLE void moveFavorite(int favoriteId, int groupId);
    Q_INVOKABLE void batchMoveFavorites(const QVariantList &favoriteIds, int groupId);   ///< 批量移动（服务端单请求原子更新），成功才发 favoriteMoved
    /// 重命名分组（成功重拉分组列表；失败 favoriteOperationFailed）。
    Q_INVOKABLE void updateFavoriteGroupName(int groupId, const QString &name);

signals:
    // 注：*Changed / *Loading 命名的信号均为对应 Q_PROPERTY 的 NOTIFY 伴侣，不再逐一注释。

    // ===== 列表 / 详情 =====
    void recipesChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    void healthFilterAppliedChanged();
    void recipeDetailChanged();
    void detailLoadingChanged();
    void detailLoadFailedChanged();

    // ===== 搜索 =====
    void searchResultsChanged();
    void searchLoadingChanged();
    void searchHasMoreChanged();
    void searchPerformedChanged();
    void searchErrorOccurred(const QString &error);   ///< 搜索失败（含续页）；不发 errorOccurred

    // ===== 营养 / 视频 =====
    void nutritionReportChanged();
    void nutritionLoadingChanged();
    void recipeVideosChanged();
    void videosLoadingChanged();

    // ===== 评分评论 =====
    void recipeRatingsChanged();
    void ratingsLoadingChanged();
    void ratingsHasMoreChanged();
    void myRatingChanged();
    void ratingSubmitted();                    ///< rateRecipe 成功
    void ratingUpdated(int ratingId);          ///< updateRating 成功
    void ratingDeleted(int ratingId);          ///< deleteRating 成功（乐观删除的确认）
    void ratingError(const QString& error);    ///< 评分/改评/删评失败；不发 errorOccurred

    // ===== 投稿与编辑 =====
    void recipeSubmitted(int id, const QString& status);   ///< 投稿成功（status=服务端审核状态）
    void submitFailed(const QString& error);
    void recipeImageUploaded(const QString& imageUrl);   ///< imageUrl=服务端返回的资源地址
    void recipeImageUploadFailed(const QString& error);
    void stepImageUploaded(int stepIndex, const QString& imageUrl);   ///< stepIndex 原样带回
    void stepImageUploadFailed(int stepIndex, const QString& error);
    void recipeEdited();
    void editFailed(const QString& error);
    void editFormDataReady();     ///< loadRecipeForEdit 成功、表单数据就绪

    // ===== 我的投稿 / 我的评论 =====
    void recipeDeleted();                     ///< 乐观删除已生效（先于服务器确认；失败随后回滚并发 deleteFailed）
    void deleteFailed(const QString& error);  ///< 删除失败（列表已回滚）
    void myRecipesChanged();
    void myRecipesLoadingChanged();
    void myRecipesHasMoreChanged();
    void myRatingsChanged();
    void myRatingsLoadingChanged();
    void myRatingsHasMoreChanged();

    // ===== 收藏 =====
    void favoritesChanged();
    void favoritesHasMoreChanged();
    void favoritesLoadingChanged();
    void favoritesLoadFailedChanged();
    void favoriteGroupsChanged();
    /// 收藏操作成功（服务器确认）。isFavorited 恒 true 为占位，方向（收藏/取消）由页面自记；
    /// 失败发 favoriteOperationFailed，页面据此回滚乐观 UI。
    void favoriteToggleSuccess(int recipeId, bool isFavorited);
    /// 取消收藏成功（removeFavorite / batchRemoveFavorites）。页面需自行刷新列表与分组。
    void favoriteRemoved();
    /// 移动成功（moveFavorite / batchMoveFavorites）。服务端单请求原子更新，成功才发。
    void favoriteMoved();
    void favoriteGroupCreated();     ///< 创建成功（分组列表异步重拉中，以 favoriteGroupsChanged 为准）
    void favoriteGroupDeleted();     ///< 乐观删除分组的确认
    /// 收藏写操作失败（收藏/取消/移动/分组增删改名）；error 为空时 VM 已兜底文案。
    void favoriteOperationFailed(const QString &error);

    // ===== 通用错误 =====
    /// 列表 / 编辑取数 / 我的投稿 / 我的评论 / 营养报告 加载失败。
    /// 不含：搜索、评分、收藏（各有专属通道）；视频、评分列表、我的评分查询静默。
    void errorOccurred(const QString &error);

private:
    enum class LoadMode { Public, Recommended };   ///< m_currentMode 的取值
    LoadMode m_currentMode = LoadMode::Public;   ///< 当前列表模式（refresh / loadNextPage 据此分派）

    void setHealthFilterApplied(bool applied);   ///< 更新 m_healthFilterApplied（变化时发信号）

    IGoCookApi *m_api;   ///< API 门面（构造注入）
    QVariantList m_recipes;   ///< 列表数据（公开/推荐共用）
    QVariantMap m_recipeDetail;   ///< 详情与编辑表单数据
    bool m_isLoading = false;   ///< 列表请求在途（公开/推荐共用）
    bool m_hasMore = false;   ///< 列表还有下一页（仅公开模式会为 true）
    bool m_healthFilterApplied = false;   ///< 推荐结果已应用健康过滤
    bool m_detailLoading = false;   ///< 详情/编辑取数在途
    bool m_detailLoadFailed = false;   ///< 详情加载失败且无缓存（页面显示居中离线视图）
    int m_detailRequestedId = -1;      ///< 详情族共用：非当前菜谱的迟到响应被丢弃（A→B 竞态防护）——
                                       ///< “目标键”语义（同菜谱兄弟请求互不作废），不同于轮次代次，不做统一
    int m_currentPage = 1;   ///< 公开列表当前页
    int m_pageSize = kPublicPageSize;   ///< 公开列表固定页大小（不随 loadPublicRecipes 的 size 参数更新）
    int m_totalPages = 0;    ///< 公开列表总页数

    // 搜索状态
    QVariantList m_searchResults;   ///< 搜索结果数据
    bool m_searchLoading = false;   ///< 搜索请求在途
    bool m_searchHasMore = false;   ///< 搜索还有下一页
    bool m_searchPerformed = false;   ///< 已执行过搜索（区分空态与无结果）
    int m_searchPage = 1;   ///< 搜索当前页
    int m_searchPageSize = kPageSize;   ///< 首屏请求的每页数量：续页须沿用同一 size（服务端 offset=(page-1)*size）
    int m_searchTotalPages = 0;   ///< 搜索总页数
    QString m_lastKeyword;   ///< 上一次搜索词（searchNextPage 据此续页）
    /// 搜索轮次（RequestGuards.h::RequestEpoch）：首屏 begin 取票据、续页沿用当前票据；
    /// 换词/清空后旧响应到达时票据已失效，静默丢弃（防旧词结果覆盖新词）
    RequestEpoch m_searchEpoch;

    // 营养报告状态
    QVariantMap m_nutritionReport;   ///< 营养报告数据（仅成功时更新）
    bool m_nutritionLoading = false;   ///< 营养报告请求在途

    // 视频状态
    QVariantList m_recipeVideos;   ///< 视频列表（失败静默）
    bool m_videosLoading = false;   ///< 视频请求在途

    // 评分评论状态
    QVariantList m_recipeRatings;   ///< 详情页评分列表
    bool m_ratingsLoading = false;   ///< 评分列表请求在途
    bool m_ratingsHasMore = false;   ///< 评分列表还有下一页
    QVariantMap m_myRating;   ///< 我的评分（空=未评分）
    int m_ratingsPage = 1;   ///< 评分列表当前页
    int m_ratingsTotalPages = 0;   ///< 评分列表总页数
    int m_ratingsRecipeId = 0;   ///< 评分列表所属菜谱（loadMoreRatings 据此续页）

    // 我的投稿状态
    QVariantList m_myRecipes;   ///< 我的投稿列表
    bool m_myRecipesLoading = false;   ///< 投稿列表请求在途
    bool m_myRecipesHasMore = false;   ///< 投稿列表还有下一页
    int m_myRecipesPage = 1;   ///< 投稿列表当前页
    int m_myRecipesTotalPages = 0;   ///< 投稿列表总页数
    QString m_myRecipesStatus;   ///< 当前投稿筛选 status（删除成功后按其重拉）
    QSet<int> m_pendingDeleteIds;  ///< 乐观删除中但 API 尚未返回的菜谱 ID

    // 我的评论状态
    QVariantList m_myRatings;   ///< 我的评论列表
    bool m_myRatingsLoading = false;   ///< 评论列表请求在途
    bool m_myRatingsHasMore = false;   ///< 评论列表还有下一页
    int m_myRatingsPage = 1;   ///< 评论列表当前页
    int m_myRatingsTotalPages = 0;   ///< 评论列表总页数

    // 收藏状态
    QVariantList m_favorites;   ///< 收藏列表（当前分组筛选）
    QVariantList m_favoriteGroups;   ///< 收藏分组（含合成默认组）
    int m_favoritesPage = 1;   ///< 收藏列表当前页
    int m_favoritesPageSize = kPageSize;   ///< 首屏加载的每页数量：续页须沿用同一 size（服务端 offset=(page-1)*size）
    QString m_favoritesGroupFilter;    ///< 当前筛选分组（""=全部）：续页须沿用，否则加载到未筛选数据
    int m_favoritesTotalPages = 0;   ///< 收藏列表总页数
    bool m_favoritesHasMore = false;   ///< 收藏还有下一页
    bool m_favoritesLoading = false;   ///< 收藏请求在途
    bool m_favoritesLoadFailed = false; ///< 收藏加载失败（页面显示居中离线视图或静默保留旧数据）
};
