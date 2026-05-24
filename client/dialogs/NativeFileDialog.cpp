#include "NativeFileDialog.h"
#include <QFileDialog>
#include <QUrl>

NativeFileDialog::NativeFileDialog(QObject *parent)
    : QObject(parent) {}

void NativeFileDialog::open()
{
    openWithFilter(
        QStringLiteral("选择文件"),
        QStringLiteral("所有文件 (*)"));
}

void NativeFileDialog::openWithFilter(const QString& title,
                                       const QString& filter)
{
    QString selectedFile = QFileDialog::getOpenFileName(
        nullptr,                       // 父窗口句柄（QML 窗口不可直接传递）
        title,
        QString(),                     // 起始目录
        filter);

    if (selectedFile.isEmpty()) {
        emit rejected();
    } else {
        emit fileSelected(selectedFile);
    }
}
