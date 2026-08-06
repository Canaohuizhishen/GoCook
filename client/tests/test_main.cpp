#include <gtest/gtest.h>
#include <QCoreApplication>

/// 客户端测试入口：必须持有 QCoreApplication 实例——
/// Qt SQL 驱动（QSQLITE 插件）的加载依赖 QCoreApplication，缺失时 QSqlDatabase::open() 会崩溃
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
