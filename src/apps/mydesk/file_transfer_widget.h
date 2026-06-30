#pragma once

#include <QWidget>
#include <QString>

#include <cstdint>

class QListWidget;
class QPushButton;
class QLabel;
class QProgressBar;

namespace rd {
class FileTransferManager;
}

// 文件传输面板 - 支持拖拽发送文件、选择保存目录
class FileTransferWidget : public QWidget {
    Q_OBJECT
public:
    explicit FileTransferWidget(QWidget* parent = nullptr);

    void setTransferManager(rd::FileTransferManager* mgr);

protected:
    // 拖拽支持
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragLeaveEvent(QDragLeaveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onSendFileClicked();
    void onFileOffered(uint32_t id, const QString& fileName, uint64_t fileSize);
    void onTransferProgress(uint32_t id, uint64_t transferred, uint64_t total);
    void onTransferCompleted(uint32_t id);
    void onTransferFailed(uint32_t id, const QString& reason);

private:
    void updateTransferList();
    QString formatSize(uint64_t bytes) const;

    rd::FileTransferManager* transferMgr_ = nullptr;
    QListWidget* transferList_ = nullptr;
    QPushButton* sendBtn_ = nullptr;
    QLabel* dropHint_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    bool isDragOver_ = false;
};
