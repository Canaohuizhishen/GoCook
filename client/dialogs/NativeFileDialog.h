#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

/**
 * @brief 原生系统文件对话框桥接
 *
 * Qt Quick 的 FileDialog 在非桌面环境下显示的是 Qt 自绘的简陋对话框。
 * 此桥接类调用 QFileDialog::getOpenFileName()，弹出操作系统原生的文件选择窗口。
 *
 * 用法（QML）：
 *   NativeFileDialog { onFileSelected: path => doUpload(path) }
 */
class NativeFileDialog : public QObject
{
    Q_OBJECT
    // 声明式注册（qt_add_qml_module 模块内 → qmltypes/qmldir 单一来源，qmllint 可静态解析）
    QML_ELEMENT
public:
    explicit NativeFileDialog(QObject *parent = nullptr);

    Q_INVOKABLE void open();
    Q_INVOKABLE void openWithFilter(const QString& title,
                                    const QString& filter);

signals:
    /// 用户选择了文件
    void fileSelected(const QString& localPath);
    /// 用户取消了选择
    void rejected();
};
