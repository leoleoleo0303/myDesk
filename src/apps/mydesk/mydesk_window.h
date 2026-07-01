#pragma once

#include <memory>
#include <thread>

#include "account_manager.h"
#include "device_id_gen.h"
#include "device_list_manager.h"
#include "frameless_window.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QStackedWidget;
class QTabWidget;
class QListWidget;
class QTimer;
class ViewerWidget;

namespace rd {
class HostService;
class ViewerSession;
class FileTransferManager;
class AudioManager;
class WebServer;
class LogPanel;
class ScreenRecorder;
}  // namespace rd

class FileTransferWidget;

// Main window - ToDesk-like dual panel design:
// Left: local device info (ID + password), host status
// Right: remote connection panel (enter remote ID + password)
// After connecting, switches to remote desktop display
//
// 新增功能:
// - 账号登录 (开发者模式/用户模式)
// - 开发者日志面板 (一键复制)
// - 文件传输 (拖拽支持)
// - 语音通话
// - 设备列表 (一键连接)
// - Web 远程服务
class MyDeskWindow : public FramelessWindow {
    Q_OBJECT
public:
    MyDeskWindow();
    ~MyDeskWindow() override;

signals:
    void viewerConnected();
    void viewerConnectFailed(const QString& msg);

private slots:
    void onConnectClicked();
    void onViewerConnected();
    void onViewerFailed(const QString& msg);
    void onViewerDisconnected();
    void onRefreshPassword();
    void onP2PRegisterClicked();

    // 新增 slots
    void onQuickConnect(const std::string& deviceId);
    void onModeChanged(int mode);
    void onAudioToggle();
    void onWebServerToggle();
    void onRecordToggle();

private:
    QWidget* buildMainPage();
    QWidget* buildLeftPanel();
    QWidget* buildRightPanel();
    QWidget* buildDeviceListPanel();
    QWidget* buildBottomPanel();  // 日志 + 文件传输 tabs

    void updateUIForMode(rd::UserMode mode);
    void saveCurrentConnection();

    rd::DeviceIdentity identity_;
    rd::AccountManager* accountMgr_ = nullptr;
    rd::DeviceListManager* deviceListMgr_ = nullptr;

    QStackedWidget* stack_ = nullptr;

    // Left panel - local info
    QLabel* deviceIdLabel_ = nullptr;
    QLabel* passwordLabel_ = nullptr;
    QLabel* hostStatusLabel_ = nullptr;
    QLabel* p2pStatusLabel_ = nullptr;
    QPushButton* refreshPwdBtn_ = nullptr;

    // Left panel - signal server config for P2P registration
    QLineEdit* signalHostEdit_ = nullptr;
    QLineEdit* signalPortEdit_ = nullptr;
    QPushButton* p2pRegisterBtn_ = nullptr;
    QLineEdit* p2pErrorEdit_ = nullptr;

    // Right panel - connection
    QComboBox* modeBox_ = nullptr;
    QLineEdit* remoteIdEdit_ = nullptr;
    QLineEdit* remotePasswordEdit_ = nullptr;
    QLineEdit* addressEdit_ = nullptr;
    QLineEdit* portEdit_ = nullptr;
    QLineEdit* viewerSignalHostEdit_ = nullptr;
    QLineEdit* viewerSignalPortEdit_ = nullptr;
    QPushButton* connectBtn_ = nullptr;
    QLabel* connectStatus_ = nullptr;

    // 设备列表
    QListWidget* deviceListWidget_ = nullptr;
    QPushButton* removeDeviceBtn_ = nullptr;

    // 底部面板 (开发者模式)
    QTabWidget* bottomTabs_ = nullptr;
    rd::LogPanel* logPanel_ = nullptr;
    FileTransferWidget* fileTransferWidget_ = nullptr;

    // 语音控制
    QPushButton* audioBtn_ = nullptr;
    QLabel* audioStatusLabel_ = nullptr;

    // Web 服务控制
    QPushButton* webServerBtn_ = nullptr;
    QLabel* webServerStatus_ = nullptr;

    // 录屏控制
    QPushButton* recordBtn_ = nullptr;
    QLabel* recordStatusLabel_ = nullptr;
    QTimer* recordTimer_ = nullptr;

    // 用户信息
    QLabel* userInfoLabel_ = nullptr;
    QPushButton* logoutBtn_ = nullptr;
    QComboBox* modeSwitch_ = nullptr;

    // Remote display
    ViewerWidget* viewerWidget_ = nullptr;

    // Services
    rd::HostService* hostService_ = nullptr;
    rd::ViewerSession* viewerSession_ = nullptr;
    rd::FileTransferManager* fileTransferMgr_ = nullptr;
    rd::AudioManager* audioMgr_ = nullptr;
    rd::WebServer* webServer_ = nullptr;
    rd::ScreenRecorder* screenRecorder_ = nullptr;
    std::thread connectThread_;
};
