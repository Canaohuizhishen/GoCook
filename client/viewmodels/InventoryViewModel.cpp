#include "InventoryViewModel.h"
#include <DataMapper.h>
#include <QPointer>
#include "../api/HttpGoCookApi.h"

InventoryViewModel::InventoryViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QVariantList InventoryViewModel::items() const { return m_items; }
bool InventoryViewModel::isLoading() const { return m_isLoading; }
bool InventoryViewModel::hasMore() const { return m_hasMore; }

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
    if (m_isLoading) return;
    m_isLoading = true;
    emit isLoadingChanged();

    // 快照当前会话（token）：响应到达时若会话已切换（登出/换号），该在途响应属于旧账号，
    // 静默丢弃——不清状态（clearAll 与新加载已接管），避免旧账号数据串入当前界面
    const std::string tokenAtSend = m_api->authToken();
    m_api->getInventory(page, size,
                        [self = QPointer<InventoryViewModel>(this), page, tokenAtSend](bool success,
                              const gocook::models::PagedInventory& data,
                              const std::string& error) {
                            if (!self) return;
                            // 会话已切换：过期响应作废。不清数据（clearAll/新加载已接管），
                            // 仅复位加载标记防页面卡死（极端时序下 clearAll 可能尚未执行）
                            if (self->m_api->authToken() != tokenAtSend) {
                                if (self->m_isLoading) {
                                    self->m_isLoading = false;
                                    emit self->isLoadingChanged();
                                }
                                return;
                            }
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

void InventoryViewModel::updateItem(int /*itemId*/, const QString& name, double quantity,
                                     const QString& unit, const QString& expiryDate)
{
    addItem(name, quantity, unit, expiryDate);
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
