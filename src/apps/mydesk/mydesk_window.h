#pragma once

#include <memory>
#include <thread>

#include "device_id_gen.h"
#include "frameless_window.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QStackedWidget;
class ViewerWidget;

namespace rd {
class HostService;
class ViewerSession;
}  // namespace rd

// Main window - ToDesk-like dual panel design:
// Left: local device info (ID + password), host status
// Right: remote connection panel (enter remote ID + password)
// After connecting, switches to remote desktop display
//
// Now uses FramelessWindow for borderless UI with custom title bar,
// rounded corners, and drop shadow.
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

private:
    QWidget* buildMainPage();
    QWidget* buildLeftPanel();
    QWidget* buildRightPanel();

    rd::DeviceIdentity identity_;

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

    // Right panel - connection
    QComboBox* modeBox_ = nullptr;
    QLineEdit* remoteIdEdit_ = nullptr;
    QLineEdit* remotePasswordEdit_ = nullptr;
    QLineEdit* addressEdit_ = nullptr;
    QLineEdit* portEdit_ = nullptr;
    // P2P mode: signal server address for viewer side
    QLineEdit* viewerSignalHostEdit_ = nullptr;
    QLineEdit* viewerSignalPortEdit_ = nullptr;
    QPushButton* connectBtn_ = nullptr;
    QLabel* connectStatus_ = nullptr;

    // Remote display
    ViewerWidget* viewerWidget_ = nullptr;

    // Services
    rd::HostService* hostService_ = nullptr;
    rd::ViewerSession* viewerSession_ = nullptr;
    std::thread connectThread_;
};
