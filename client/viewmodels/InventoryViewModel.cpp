#include "InventoryViewModel.h"
#include <DataMapper.h>
#include <QPointer>
#include "../api/HttpGoCookApi.h"

InventoryViewModel::InventoryViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QVariantList InventoryViewModel::items() const { return m_items; }
bool InventoryViewModel::isLoading() const { return m_isLoading; }
bool InventoryViewModel::hasMore() const { return m_hasMore; }
QString InventoryViewModel::filterText() const { return m_filterText; }

void InventoryViewModel::setFilterText(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed == m_filterText) return;
    m_filterText = trimmed;
    emit filterTextChanged();
    // 回到第一页按新词加载。若上一请求仍在途，loadInventory 会递增代次并立即发出新请求
    // （不再等旧请求回归）：旧响应到达时代次落后被静默丢弃，最后一次输入必达且不押后
    loadInventory(1, m_pageSize);
}

void InventoryViewModel::refresh()
{
    m_currentPage = 1;
    m_items.clear();
    emit itemsChanged();
    m_hasMore = false;
    emit hasMoreChanged();
    loadInventory(1, m_pageSize);
}

void InventoryViewModel::clearAll()
{
    m_items.clear();
    m_hasMore = false;
    m_currentPage = 1;
    m_totalPages = 0;
    m_isLoading = false;
    ++m_epoch;  // 作废在途响应：旧账号/旧轮次的回调到达时代次落后，静默退出
    m_filterText.clear();
    emit filterTextChanged();
    emit itemsChanged();
    emit hasMoreChanged();
    emit isLoadingChanged();
}

void InventoryViewModel::loadNextPage()
{
    if (m_isLoading || !m_hasMore) return;
    loadInventory(m_currentPage + 1, m_pageSize);
}

void InventoryViewModel::loadInventory(int page, int size)
{
    // 翻页（page>1）是当前轮次的延续：仅当无在途请求时才发出，防止翻页请求堆叠。
    // 首屏/换词/刷新（page<=1）= 开启新一轮：递增代次并立即发送，不被在途请求阻塞——
    // 旧响应到达时代次已落后，由回调侧静默丢弃（见下），无需此处等待或事后补发
    // 注：page>1 的唯一入口是 loadNextPage()（其已前置 m_isLoading 拦截），本守卫为防御性
    if (page > 1 && m_isLoading) return;
    if (page <= 1) ++m_epoch;
    m_isLoading = true;
    emit isLoadingChanged();

    // 快照当前会话（token）与请求代次：响应到达时若会话已切换（登出/换号），该在途响应属于旧账号——
    // 静默丢弃，避免旧账号数据串入当前界面；会话未变但代次已落后（在途期间换词/刷新/clearAll），
    // 本响应已被更新的请求取代——同样静默丢弃（数据由最新请求填充，避免旧词结果闪回）
    const std::string tokenAtSend = m_api->authToken();
    const int epochAtSend = m_epoch;
    m_api->getInventory(page, size, m_filterText.toStdString(),
                        [self = QPointer<InventoryViewModel>(this), page, tokenAtSend, epochAtSend](bool success,
                              const gocook::models::PagedInventory& data,
                              const std::string& error) {
                            if (!self) return;
                            // 会话已切换：过期响应作废。仅当本响应仍属最新代次（无新请求接管、
                            // clearAll 未执行）时复位加载标记防页面卡死；否则状态由新请求/clearAll 管理
                            if (self->m_api->authToken() != tokenAtSend) {
                                if (self->m_epoch == epochAtSend && self->m_isLoading) {
                                    self->m_isLoading = false;
                                    emit self->isLoadingChanged();
                                }
                                return;
                            }
                            // 代次落后：本响应发送后又有更新的请求发出（换词/刷新/clearAll 均会递增）——
                            // 本响应已过时，静默丢弃：不落数据、不动加载标记（最新请求自行收尾）
                            if (epochAtSend != self->m_epoch) return;
                            if (!success) {
                                // 守卫拦截（未登录，error=请先登录）：清空上一登录态残留数据，
                                // 避免游客看到旧库存；网络错误保留旧数据（页面静默显示）
                                if (QString::fromStdString(error) == HttpGoCookApi::kAuthRequiredError) {
                                    self->m_items.clear();
                                    self->m_hasMore = false;
                                    self->m_currentPage = 1;
                                    emit self->itemsChanged();
                                    emit self->hasMoreChanged();
                                }
                                emit self->errorOccurred(QString::fromStdString(error));
                                self->m_isLoading = false;
                                emit self->isLoadingChanged();
                                return;
                            }

                            if (page == 1)
                                self->m_items.clear();

                            for (const auto& item : data.data)
                                self->m_items.append(DataMapper::toMap(item));

                            self->m_currentPage = data.pagination.page;
                            self->m_totalPages = data.pagination.total_pages;
                            self->m_hasMore = (self->m_currentPage < self->m_totalPages);

                            emit self->itemsChanged();
                            emit self->hasMoreChanged();
                            self->m_isLoading = false;
                            emit self->isLoadingChanged();
                        });
}

void InventoryViewModel::addItem(const QString& name, double quantity,
                                  const QString& unit, const QString& expiryDate)
{
    gocook::models::UpsertInventoryRequest req;
    req.ingredient_name = name.toStdString();
    req.quantity = quantity;
    req.unit = unit.toStdString();
    if (!expiryDate.isEmpty())
        req.expiry_date = expiryDate.toStdString();

    m_api->upsertInventory(req, [self = QPointer<InventoryViewModel>(this)](bool success, int id, const std::string& error) {
        if (!self) return;
        if (success) {
            self->refresh();
        } else {
            emit self->errorOccurred(QString::fromStdString(error));
        }
    });
}

void InventoryViewModel::updateItem(int itemId, const QString& name, double quantity,
                                     const QString& unit, const QString& expiryDate)
{
    gocook::models::UpsertInventoryRequest req;
    req.ingredient_name = name.toStdString();
    req.quantity = quantity;
    req.unit = unit.toStdString();
    if (!expiryDate.isEmpty())
        req.expiry_date = expiryDate.toStdString();

    // 编辑 = 按 id 整行替换（PUT /api/inventory/:id）：改名/改单位/改数量均为替换而非累加
    // （决策 2026-09-04：添加页走 POST 累加，编辑入口走 PUT 替换）
    m_api->updateInventoryItem(itemId, req,
        [self = QPointer<InventoryViewModel>(this)](bool success, const std::string& error) {
            if (!self) return;
            if (success) {
                self->refresh();
            } else {
                emit self->errorOccurred(QString::fromStdString(error));
            }
        });
}

void InventoryViewModel::deleteItem(int itemId)
{
    // 乐观删除：保存旧列表用于回滚
    QVariantList oldItems = m_items;

    // 1. 立即从列表中移除
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].toMap()["id"].toInt() == itemId) {
            m_items.removeAt(i);
            emit itemsChanged();
            break;
        }
    }

    // 2. 发 API 请求
    m_api->deleteInventoryItem(itemId,
        [self = QPointer<InventoryViewModel>(this), itemId, oldItems]
        (bool success, const std::string& error) {
            if (!self) return;
            if (!success) {
                // 失败 → 回滚
                self->m_items = oldItems;
                emit self->itemsChanged();
                emit self->errorOccurred(QString::fromStdString(error));
            }
        });
}
