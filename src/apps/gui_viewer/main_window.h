#pragma once

#include <QWidget>

#include <memory>
#include <thread>

#include "core/net/channel.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QStackedWidget;
class VideoWidget;

namespace rd {
class RemoteSession;
}

// 主窗口：连接表单 + 远程画面两个页面。
class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;

signals:
    void established();
    void failed(const QString& msg);

private slots:
    void onConnectClicked();
    void onEstablished();
    void onFailed(const QString& msg);
    void onDisconnected();

private:
    QWidget* buildConnectPage();

    QComboBox* modeBox_ = nullptr;
    QLineEdit* addrEdit_ = nullptr;
    QLineEdit* portEdit_ = nullptr;
    QLineEdit* idEdit_ = nullptr;
    QPushButton* connectBtn_ = nullptr;
    QLabel* status_ = nullptr;

    QStackedWidget* stack_ = nullptr;
    VideoWidget* video_ = nullptr;
    rd::RemoteSession* session_ = nullptr;

    std::thread connectThread_;
    std::unique_ptr<rd::IChannel> pendingChannel_;
};
