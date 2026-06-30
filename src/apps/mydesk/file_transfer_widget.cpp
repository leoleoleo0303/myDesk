#include "file_transfer_widget.h"
#include "core/file_transfer/file_transfer.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

FileTransferWidget::FileTransferWidget(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // 标题
    auto* titleLabel = new QLabel("File Transfer");
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #2c3e50;");
    layout->addWidget(titleLabel);

    // 拖拽区域提示
    dropHint_ = new QLabel("📁 Drag files here to send\nor click 'Send File' button");
    dropHint_->setAlignment(Qt::AlignCenter);
    dropHint_->setMinimumHeight(80);
    dropHint_->setStyleSheet(
        "QLabel { background-color: #f0f8ff; border: 2px dashed #3498db; "
        "border-radius: 8px; color: #3498db; font-size: 13px; padding: 15px; }");
    layout->addWidget(dropHint_);

    // 发送按钮
    auto* btnRow = new QHBoxLayout;
    sendBtn_ = new QPushButton("📤 Send File");
    sendBtn_->setMinimumHeight(36);
    sendBtn_->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; "
        "border-radius: 4px; font-weight: bold; padding: 5px 15px; }"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:disabled { background-color: #bdc3c7; }");
    connect(sendBtn_, &QPushButton::clicked, this, &FileTransferWidget::onSendFileClicked);
    btnRow->addWidget(sendBtn_);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    // 传输列表
    transferList_ = new QListWidget;
    transferList_->setStyleSheet(
        "QListWidget { border: 1px solid #dfe6e9; border-radius: 4px; "
        "background: white; font-size: 12px; }"
        "QListWidget::item { padding: 8px; border-bottom: 1px solid #ecf0f1; }");
    transferList_->setMinimumHeight(150);
    layout->addWidget(transferList_);

    // 状态栏
    statusLabel_ = new QLabel("No active transfers");
    statusLabel_->setStyleSheet("color: #7f8c8d; font-size: 11px;");
    layout->addWidget(statusLabel_);
}

void FileTransferWidget::setTransferManager(rd::FileTransferManager* mgr) {
    transferMgr_ = mgr;
    if (!mgr) return;

    connect(mgr, &rd::FileTransferManager::fileOffered,
            this, &FileTransferWidget::onFileOffered);
    connect(mgr, &rd::FileTransferManager::transferProgress,
            this, &FileTransferWidget::onTransferProgress);
    connect(mgr, &rd::FileTransferManager::transferCompleted,
            this, &FileTransferWidget::onTransferCompleted);
    connect(mgr, &rd::FileTransferManager::transferFailed,
            this, &FileTransferWidget::onTransferFailed);
}

void FileTransferWidget::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
        isDragOver_ = true;
        dropHint_->setStyleSheet(
            "QLabel { background-color: #d4efdf; border: 2px dashed #27ae60; "
            "border-radius: 8px; color: #27ae60; font-size: 13px; padding: 15px; }");
        dropHint_->setText("📥 Release to send file(s)");
    }
}

void FileTransferWidget::dragLeaveEvent(QDragLeaveEvent* /*e*/) {
    isDragOver_ = false;
    dropHint_->setStyleSheet(
        "QLabel { background-color: #f0f8ff; border: 2px dashed #3498db; "
        "border-radius: 8px; color: #3498db; font-size: 13px; padding: 15px; }");
    dropHint_->setText("📁 Drag files here to send\nor click 'Send File' button");
}

void FileTransferWidget::dropEvent(QDropEvent* e) {
    isDragOver_ = false;
    dropHint_->setStyleSheet(
        "QLabel { background-color: #f0f8ff; border: 2px dashed #3498db; "
        "border-radius: 8px; color: #3498db; font-size: 13px; padding: 15px; }");
    dropHint_->setText("📁 Drag files here to send\nor click 'Send File' button");

    if (!transferMgr_) {
        statusLabel_->setText("Not connected - cannot send files");
        return;
    }

    const QMimeData* mimeData = e->mimeData();
    if (mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                std::string path = url.toLocalFile().toStdString();
                uint32_t id = transferMgr_->offerFile(path);
                if (id > 0) {
                    auto* item = new QListWidgetItem(
                        QString("📤 Sending: %1").arg(url.fileName()));
                    transferList_->addItem(item);
                    statusLabel_->setText(QString("Offering file: %1").arg(url.fileName()));
                }
            }
        }
    }
    e->acceptProposedAction();
}

void FileTransferWidget::onSendFileClicked() {
    if (!transferMgr_) {
        statusLabel_->setText("Not connected - cannot send files");
        return;
    }

    QStringList files = QFileDialog::getOpenFileNames(this, "Select Files to Send");
    for (const QString& file : files) {
        uint32_t id = transferMgr_->offerFile(file.toStdString());
        if (id > 0) {
            QFileInfo fi(file);
            auto* item = new QListWidgetItem(
                QString("📤 Sending: %1 (%2)").arg(fi.fileName(), formatSize(fi.size())));
            transferList_->addItem(item);
        }
    }
}

void FileTransferWidget::onFileOffered(uint32_t id, const QString& fileName, uint64_t fileSize) {
    // 收到对端发送文件请求，让用户选择保存路径
    QString msg = QString("Remote wants to send file:\n%1 (%2)\n\nAccept?")
                      .arg(fileName, formatSize(fileSize));

    auto result = QMessageBox::question(this, "File Transfer Request", msg,
                                        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        QString savePath = QFileDialog::getSaveFileName(this, "Save File As", fileName);
        if (!savePath.isEmpty()) {
            transferMgr_->acceptFile(id, savePath.toStdString());
            auto* item = new QListWidgetItem(
                QString("📥 Receiving: %1").arg(fileName));
            transferList_->addItem(item);
            statusLabel_->setText(QString("Receiving: %1").arg(fileName));
        } else {
            transferMgr_->rejectFile(id);
        }
    } else {
        transferMgr_->rejectFile(id);
    }
}

void FileTransferWidget::onTransferProgress(uint32_t /*id*/, uint64_t transferred, uint64_t total) {
    if (total > 0) {
        int percent = static_cast<int>(transferred * 100 / total);
        statusLabel_->setText(QString("Transfer: %1 / %2 (%3%)")
                                  .arg(formatSize(transferred), formatSize(total))
                                  .arg(percent));
    }
}

void FileTransferWidget::onTransferCompleted(uint32_t /*id*/) {
    auto* item = new QListWidgetItem("✅ Transfer completed");
    transferList_->addItem(item);
    statusLabel_->setText("Transfer completed successfully");
}

void FileTransferWidget::onTransferFailed(uint32_t /*id*/, const QString& reason) {
    auto* item = new QListWidgetItem(QString("❌ Failed: %1").arg(reason));
    transferList_->addItem(item);
    statusLabel_->setText(QString("Transfer failed: %1").arg(reason));
}

QString FileTransferWidget::formatSize(uint64_t bytes) const {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024ull * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
}
