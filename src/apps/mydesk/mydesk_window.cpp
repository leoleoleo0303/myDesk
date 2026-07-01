#include "mydesk_window.h"

#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "account_manager.h"
#include "core/audio/audio_manager.h"
#include "core/capture/screen_recorder.h"
#include "core/file_transfer/file_transfer.h"
#include "device_list_manager.h"
#include "file_transfer_widget.h"
#include "host_service.h"
#include "log_panel.h"
#include "login_dialog.h"
#include "viewer_session.h"
#include "viewer_widget.h"
#include "web_server.h"

MyDeskWindow::MyDeskWindow() {
    setWindowTitle("myDesk - Remote Desktop");
    resize(1200, 780);
    setMinimumSize(1000, 700);

    // 初始化管理器
    accountMgr_ = new rd::AccountManager(this);
    deviceListMgr_ = new rd::DeviceListManager(this);
    fileTransferMgr_ = new rd::FileTransferManager(this);
    audioMgr_ = new rd::AudioManager(this);
    webServer_ = new rd::WebServer(this);
    screenRecorder_ = new rd::ScreenRecorder();

    identity_ = rd::DeviceIdentity::load();

    // 显示登录对话框
    rd::LoginDialog loginDlg(accountMgr_, this);
    loginDlg.exec();  // 阻塞直到登录成功或跳过

    // 构建主界面
    stack_ = new QStackedWidget;
    setCentralWidget(stack_);

    stack_->addWidget(buildMainPage());

    viewerWidget_ = new ViewerWidget;
    stack_->addWidget(viewerWidget_);

    // Start host service
    hostService_ = new rd::HostService(this);
    hostService_->setPassword(identity_.password);
    hostService_->startListening(9000);

    // 设置日志面板为全局实例
    rd::setGlobalLogPanel(logPanel_);
    LOG_INFO("myDesk started");
    LOG_INFO(QString("Device ID: %1").arg(QString::fromStdString(identity_.deviceId)));
    LOG_INFO(QString("User: %1 (Mode: %2)")
                 .arg(QString::fromStdString(accountMgr_->accountInfo().username),
                      accountMgr_->userMode() == rd::UserMode::Developer ? "Developer" : "User"));

    // 连接 host service 信号
    connect(hostService_, &rd::HostService::clientConnected, this,
            [this](const QString& info) {
                hostStatusLabel_->setText("● Someone is controlling this PC");
                hostStatusLabel_->setStyleSheet("color: #e74c3c; font-weight: bold;");
                LOG_INFO(QString("Client connected: %1").arg(info));
            });
    connect(hostService_, &rd::HostService::clientDisconnected, this,
            [this]() {
                hostStatusLabel_->setText("● Waiting for connection...");
                hostStatusLabel_->setStyleSheet("color: #27ae60;");
                LOG_INFO("Client disconnected");
            });
    connect(hostService_, &rd::HostService::p2pRegistered, this,
            [this]() {
                p2pStatusLabel_->setText("● P2P registered (ready)");
                p2pStatusLabel_->setStyleSheet("color: #27ae60;");
                p2pErrorEdit_->setVisible(false);
                p2pRegisterBtn_->setText("Re-register");
                p2pRegisterBtn_->setEnabled(true);
                LOG_INFO("P2P registered successfully");
            });
    connect(hostService_, &rd::HostService::p2pRegisterFailed, this,
            [this](const QString& msg) {
                p2pStatusLabel_->setText("● P2P: " + msg);
                p2pStatusLabel_->setStyleSheet("color: #e74c3c;");
                p2pErrorEdit_->setText(msg);
                p2pErrorEdit_->setVisible(true);
                p2pRegisterBtn_->setEnabled(true);
                LOG_ERROR(QString("P2P registration failed: %1").arg(msg));
            });

    connect(this, &MyDeskWindow::viewerConnected, this,
            &MyDeskWindow::onViewerConnected);
    connect(this, &MyDeskWindow::viewerConnectFailed, this,
            &MyDeskWindow::onViewerFailed);

    // Web server signals
    connect(webServer_, &rd::WebServer::started, this, [this](uint16_t port) {
        webServerStatus_->setText(QString("● Web: http://localhost:%1").arg(port));
        webServerStatus_->setStyleSheet("color: #27ae60;");
        webServerBtn_->setText("Stop Web");
        LOG_INFO(QString("Web server started on port %1").arg(port));
    });
    connect(webServer_, &rd::WebServer::clientConnected, this, [this](const QString& addr) {
        LOG_INFO(QString("Web client connected: %1").arg(addr));
    });

    // 根据模式调整界面
    updateUIForMode(accountMgr_->userMode());
}

MyDeskWindow::~MyDeskWindow() {
    if (connectThread_.joinable()) connectThread_.join();
    if (viewerSession_) viewerSession_->stop();
    hostService_->stop();
    webServer_->stop();
    audioMgr_->stopCapture();
    audioMgr_->stopPlayback();
    if (screenRecorder_) {
        screenRecorder_->stop();
        delete screenRecorder_;
        screenRecorder_ = nullptr;
    }
    rd::setGlobalLogPanel(nullptr);
}

QWidget* MyDeskWindow::buildMainPage() {
    auto* page = new QWidget;
    auto* mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 顶部用户信息栏
    auto* topBar = new QWidget;
    topBar->setStyleSheet("background-color: #f8f9fa; border-bottom: 1px solid #dfe6e9;");
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(15, 5, 15, 5);

    userInfoLabel_ = new QLabel(QString("👤 %1").arg(
        QString::fromStdString(accountMgr_->accountInfo().username)));
    userInfoLabel_->setStyleSheet("font-size: 12px; color: #2c3e50;");
    topLayout->addWidget(userInfoLabel_);

    modeSwitch_ = new QComboBox;
    modeSwitch_->addItem("Developer Mode");
    modeSwitch_->addItem("User Mode");
    modeSwitch_->setCurrentIndex(static_cast<int>(accountMgr_->userMode()));
    modeSwitch_->setFixedWidth(140);
    modeSwitch_->setStyleSheet("font-size: 11px;");
    connect(modeSwitch_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MyDeskWindow::onModeChanged);
    topLayout->addWidget(modeSwitch_);

    topLayout->addStretch();

    // Web server control
    webServerStatus_ = new QLabel("● Web: off");
    webServerStatus_->setStyleSheet("color: #999; font-size: 11px;");
    topLayout->addWidget(webServerStatus_);

    webServerBtn_ = new QPushButton("Start Web");
    webServerBtn_->setFixedHeight(26);
    webServerBtn_->setStyleSheet(
        "QPushButton { background-color: #9b59b6; color: white; "
        "border-radius: 3px; font-size: 11px; padding: 2px 8px; }"
        "QPushButton:hover { background-color: #8e44ad; }");
    connect(webServerBtn_, &QPushButton::clicked, this, &MyDeskWindow::onWebServerToggle);
    topLayout->addWidget(webServerBtn_);

    topLayout->addSpacing(10);

    logoutBtn_ = new QPushButton("Logout");
    logoutBtn_->setFixedHeight(26);
    logoutBtn_->setStyleSheet(
        "QPushButton { background: transparent; color: #e74c3c; "
        "border: 1px solid #e74c3c; border-radius: 3px; font-size: 11px; padding: 2px 8px; }"
        "QPushButton:hover { background-color: #ffeaea; }");
    connect(logoutBtn_, &QPushButton::clicked, this, [this]() {
        accountMgr_->logout();
        // 重新显示登录框
        rd::LoginDialog dlg(accountMgr_, this);
        dlg.exec();
        userInfoLabel_->setText(QString("👤 %1").arg(
            QString::fromStdString(accountMgr_->accountInfo().username)));
    });
    topLayout->addWidget(logoutBtn_);

    mainLayout->addWidget(topBar);

    // 中间区域: 左中右 splitter
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setContentsMargins(10, 10, 10, 10);

    // 左侧面板
    splitter->addWidget(buildLeftPanel());

    // 中间连接面板
    splitter->addWidget(buildRightPanel());

    // 右侧设备列表
    splitter->addWidget(buildDeviceListPanel());

    splitter->setSizes({300, 350, 250});
    mainLayout->addWidget(splitter, 1);

    // 底部面板 (开发者模式可见)
    mainLayout->addWidget(buildBottomPanel());

    return page;
}

QWidget* MyDeskWindow::buildLeftPanel() {
    auto* group = new QGroupBox("This Device");
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(12);

    auto* idTitle = new QLabel("Device ID");
    idTitle->setStyleSheet("font-size: 12px; color: #666;");
    layout->addWidget(idTitle);

    deviceIdLabel_ = new QLabel(QString::fromStdString(identity_.deviceId));
    QFont idFont;
    idFont.setPointSize(24);
    idFont.setBold(true);
    deviceIdLabel_->setFont(idFont);
    deviceIdLabel_->setStyleSheet("color: #2c3e50;");
    deviceIdLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(deviceIdLabel_);

    layout->addSpacing(5);

    auto* pwdTitle = new QLabel("Temporary Password");
    pwdTitle->setStyleSheet("font-size: 12px; color: #666;");
    layout->addWidget(pwdTitle);

    auto* pwdRow = new QHBoxLayout;
    passwordLabel_ = new QLabel(QString::fromStdString(identity_.password));
    QFont pwdFont;
    pwdFont.setPointSize(18);
    pwdFont.setBold(true);
    passwordLabel_->setFont(pwdFont);
    passwordLabel_->setStyleSheet("color: #2980b9;");
    passwordLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pwdRow->addWidget(passwordLabel_);

    refreshPwdBtn_ = new QPushButton("↻");
    refreshPwdBtn_->setFixedSize(30, 30);
    refreshPwdBtn_->setToolTip("Regenerate password");
    connect(refreshPwdBtn_, &QPushButton::clicked, this,
            &MyDeskWindow::onRefreshPassword);
    pwdRow->addWidget(refreshPwdBtn_);
    pwdRow->addStretch();
    layout->addLayout(pwdRow);

    layout->addSpacing(5);

    hostStatusLabel_ = new QLabel("● Waiting for connection...");
    hostStatusLabel_->setStyleSheet("color: #27ae60;");
    layout->addWidget(hostStatusLabel_);

    layout->addSpacing(10);

    // P2P Section
    auto* p2pTitle = new QLabel("P2P (Signal Server)");
    p2pTitle->setStyleSheet("font-size: 12px; color: #666; font-weight: bold;");
    layout->addWidget(p2pTitle);

    auto* sigRow = new QHBoxLayout;
    signalHostEdit_ = new QLineEdit("127.0.0.1");
    signalHostEdit_->setPlaceholderText("Signal IP");
    signalHostEdit_->setFixedWidth(120);
    sigRow->addWidget(signalHostEdit_);
    sigRow->addWidget(new QLabel(":"));
    signalPortEdit_ = new QLineEdit("7000");
    signalPortEdit_->setFixedWidth(60);
    sigRow->addWidget(signalPortEdit_);
    sigRow->addStretch();
    layout->addLayout(sigRow);

    p2pRegisterBtn_ = new QPushButton("Register P2P");
    p2pRegisterBtn_->setFixedHeight(30);
    p2pRegisterBtn_->setStyleSheet(
        "QPushButton { background-color: #9b59b6; color: white; "
        "border-radius: 4px; font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background-color: #8e44ad; }"
        "QPushButton:disabled { background-color: #bdc3c7; }");
    connect(p2pRegisterBtn_, &QPushButton::clicked, this,
            &MyDeskWindow::onP2PRegisterClicked);
    layout->addWidget(p2pRegisterBtn_);

    p2pStatusLabel_ = new QLabel("● P2P not registered");
    p2pStatusLabel_->setStyleSheet("color: #999;");
    layout->addWidget(p2pStatusLabel_);

    p2pErrorEdit_ = new QLineEdit;
    p2pErrorEdit_->setReadOnly(true);
    p2pErrorEdit_->setVisible(false);
    p2pErrorEdit_->setStyleSheet(
        "QLineEdit { background-color: #ffeaea; color: #c0392b; "
        "border: 1px solid #e74c3c; border-radius: 3px; padding: 4px; font-size: 11px; }");
    layout->addWidget(p2pErrorEdit_);

    layout->addStretch();

    // 语音控制
    auto* audioRow = new QHBoxLayout;
    audioBtn_ = new QPushButton("🎤 Voice Call");
    audioBtn_->setFixedHeight(32);
    audioBtn_->setCheckable(true);
    audioBtn_->setStyleSheet(
        "QPushButton { background-color: #2ecc71; color: white; "
        "border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #27ae60; }"
        "QPushButton:checked { background-color: #e74c3c; }");
    connect(audioBtn_, &QPushButton::clicked, this, &MyDeskWindow::onAudioToggle);
    audioRow->addWidget(audioBtn_);
    layout->addLayout(audioRow);

    audioStatusLabel_ = new QLabel("Audio: Off");
    audioStatusLabel_->setStyleSheet("color: #999; font-size: 11px;");
    layout->addWidget(audioStatusLabel_);

    // 录屏控制
    auto* recordRow = new QHBoxLayout;
    recordBtn_ = new QPushButton("⏺ Record Screen");
    recordBtn_->setFixedHeight(32);
    recordBtn_->setCheckable(true);
    recordBtn_->setStyleSheet(
        "QPushButton { background-color: #e67e22; color: white; "
        "border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #d35400; }"
        "QPushButton:checked { background-color: #c0392b; }");
    connect(recordBtn_, &QPushButton::clicked, this, &MyDeskWindow::onRecordToggle);
    recordRow->addWidget(recordBtn_);
    layout->addLayout(recordRow);

    recordStatusLabel_ = new QLabel("Recording: Off");
    recordStatusLabel_->setStyleSheet("color: #999; font-size: 11px;");
    layout->addWidget(recordStatusLabel_);

    return group;
}

QWidget* MyDeskWindow::buildRightPanel() {
    auto* group = new QGroupBox("Remote Control");
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(10);

    modeBox_ = new QComboBox;
    modeBox_->addItem("LAN Direct");
    modeBox_->addItem("Device ID (P2P)");
    layout->addWidget(new QLabel("Connection Mode"));
    layout->addWidget(modeBox_);

    // LAN fields
    addressEdit_ = new QLineEdit("127.0.0.1");
    addressEdit_->setPlaceholderText("Remote IP address");
    layout->addWidget(new QLabel("Remote Address"));
    layout->addWidget(addressEdit_);

    portEdit_ = new QLineEdit("9000");
    portEdit_->setPlaceholderText("Port");
    layout->addWidget(new QLabel("Port"));
    layout->addWidget(portEdit_);

    // P2P fields
    remoteIdEdit_ = new QLineEdit;
    remoteIdEdit_->setPlaceholderText("Enter remote Device ID");
    QFont idEditFont;
    idEditFont.setPointSize(14);
    remoteIdEdit_->setFont(idEditFont);
    layout->addWidget(new QLabel("Remote Device ID"));
    layout->addWidget(remoteIdEdit_);

    viewerSignalHostEdit_ = new QLineEdit("127.0.0.1");
    viewerSignalHostEdit_->setPlaceholderText("Signal server IP");
    layout->addWidget(new QLabel("Signal Server"));
    layout->addWidget(viewerSignalHostEdit_);

    viewerSignalPortEdit_ = new QLineEdit("7000");
    layout->addWidget(new QLabel("Signal Port"));
    layout->addWidget(viewerSignalPortEdit_);

    // Password
    remotePasswordEdit_ = new QLineEdit;
    remotePasswordEdit_->setPlaceholderText("Enter remote password");
    remotePasswordEdit_->setEchoMode(QLineEdit::Password);
    layout->addWidget(new QLabel("Password"));
    layout->addWidget(remotePasswordEdit_);

    connectBtn_ = new QPushButton("Connect Remote Desktop");
    connectBtn_->setMinimumHeight(44);
    connectBtn_->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; "
        "border-radius: 6px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:disabled { background-color: #bdc3c7; }");
    connect(connectBtn_, &QPushButton::clicked, this,
            &MyDeskWindow::onConnectClicked);
    layout->addWidget(connectBtn_);

    connectStatus_ = new QLabel("");
    connectStatus_->setStyleSheet("color: #666;");
    layout->addWidget(connectStatus_);

    layout->addStretch();

    // Mode switching
    auto updateMode = [this](int idx) {
        const bool isLAN = (idx == 0);
        const bool isP2P = (idx == 1);
        addressEdit_->setVisible(isLAN);
        portEdit_->setVisible(isLAN);
        remoteIdEdit_->setVisible(isP2P);
        viewerSignalHostEdit_->setVisible(isP2P);
        viewerSignalPortEdit_->setVisible(isP2P);
    };
    connect(modeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, updateMode);
    updateMode(0);

    return group;
}

QWidget* MyDeskWindow::buildDeviceListPanel() {
    auto* group = new QGroupBox("Saved Devices");
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(8);

    deviceListWidget_ = new QListWidget;
    deviceListWidget_->setStyleSheet(
        "QListWidget { border: 1px solid #dfe6e9; border-radius: 4px; }"
        "QListWidget::item { padding: 8px; border-bottom: 1px solid #ecf0f1; }"
        "QListWidget::item:selected { background-color: #d6eaf8; }"
        "QListWidget::item:hover { background-color: #ebf5fb; }");

    // 加载设备列表
    for (const auto& dev : deviceListMgr_->devices()) {
        QString label = QString::fromStdString(dev.alias.empty() ? dev.deviceId : dev.alias);
        if (!dev.lastAddress.empty()) {
            label += QString(" (%1)").arg(QString::fromStdString(dev.lastAddress));
        }
        deviceListWidget_->addItem(label);
    }

    // 双击一键连接
    connect(deviceListWidget_, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) {
                int row = deviceListWidget_->row(item);
                const auto& devices = deviceListMgr_->devices();
                if (row >= 0 && row < static_cast<int>(devices.size())) {
                    onQuickConnect(devices[row].deviceId);
                }
            });
    layout->addWidget(deviceListWidget_);

    auto* btnRow = new QHBoxLayout;
    auto* quickConnBtn = new QPushButton("⚡ Quick Connect");
    quickConnBtn->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; "
        "border-radius: 3px; font-size: 11px; padding: 4px 8px; }"
        "QPushButton:hover { background-color: #229954; }");
    connect(quickConnBtn, &QPushButton::clicked, this, [this]() {
        auto* item = deviceListWidget_->currentItem();
        if (!item) return;
        int row = deviceListWidget_->row(item);
        const auto& devices = deviceListMgr_->devices();
        if (row >= 0 && row < static_cast<int>(devices.size())) {
            onQuickConnect(devices[row].deviceId);
        }
    });
    btnRow->addWidget(quickConnBtn);

    removeDeviceBtn_ = new QPushButton("🗑 Remove");
    removeDeviceBtn_->setStyleSheet(
        "QPushButton { background-color: #e74c3c; color: white; "
        "border-radius: 3px; font-size: 11px; padding: 4px 8px; }"
        "QPushButton:hover { background-color: #c0392b; }");
    connect(removeDeviceBtn_, &QPushButton::clicked, this, [this]() {
        auto* item = deviceListWidget_->currentItem();
        if (!item) return;
        int row = deviceListWidget_->row(item);
        const auto& devices = deviceListMgr_->devices();
        if (row >= 0 && row < static_cast<int>(devices.size())) {
            deviceListMgr_->removeDevice(devices[row].deviceId);
            delete deviceListWidget_->takeItem(row);
        }
    });
    btnRow->addWidget(removeDeviceBtn_);
    layout->addLayout(btnRow);

    return group;
}

QWidget* MyDeskWindow::buildBottomPanel() {
    bottomTabs_ = new QTabWidget;
    bottomTabs_->setMaximumHeight(220);
    bottomTabs_->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #dfe6e9; border-top: none; }"
        "QTabBar::tab { padding: 6px 12px; font-size: 12px; }"
        "QTabBar::tab:selected { background: white; border-bottom: 2px solid #3498db; }");

    // 日志面板
    logPanel_ = new rd::LogPanel;
    bottomTabs_->addTab(logPanel_, "📋 Logs");

    // 文件传输面板
    fileTransferWidget_ = new FileTransferWidget;
    fileTransferWidget_->setTransferManager(fileTransferMgr_);
    bottomTabs_->addTab(fileTransferWidget_, "📁 File Transfer");

    return bottomTabs_;
}

// ========== Slots ==========

void MyDeskWindow::onConnectClicked() {
    if (connectThread_.joinable()) connectThread_.join();

    const int mode = modeBox_->currentIndex();
    const std::string password = remotePasswordEdit_->text().toStdString();

    connectBtn_->setEnabled(false);
    connectStatus_->setText("Connecting...");
    connectStatus_->setStyleSheet("color: #f39c12;");
    LOG_INFO("Connecting to remote...");

    if (mode == 0) {
        const std::string addr = addressEdit_->text().toStdString();
        const uint16_t port =
            static_cast<uint16_t>(portEdit_->text().toUShort());

        LOG_DEBUG(QString("LAN connect: %1:%2").arg(
            QString::fromStdString(addr)).arg(port));

        connectThread_ = std::thread([this, addr, port, password]() {
            auto session = new rd::ViewerSession(nullptr);
            if (session->connectLAN(addr, port, password)) {
                viewerSession_ = session;
                emit viewerConnected();
            } else {
                delete session;
                emit viewerConnectFailed("LAN connection failed");
            }
        });
    } else {
        const std::string deviceId = remoteIdEdit_->text().toStdString();
        const std::string sigHost = viewerSignalHostEdit_->text().toStdString();
        const uint16_t sigPort = static_cast<uint16_t>(
            viewerSignalPortEdit_->text().toUShort());

        LOG_DEBUG(QString("P2P connect: device=%1 via %2:%3").arg(
            QString::fromStdString(deviceId),
            QString::fromStdString(sigHost)).arg(sigPort));

        connectThread_ = std::thread(
            [this, sigHost, sigPort, deviceId, password]() {
                auto session = new rd::ViewerSession(nullptr);
                if (session->connectP2P(sigHost, sigPort, deviceId, password)) {
                    viewerSession_ = session;
                    emit viewerConnected();
                } else {
                    delete session;
                    emit viewerConnectFailed("P2P connection failed");
                }
            });
    }
}

void MyDeskWindow::onViewerConnected() {
    connectStatus_->setText("Connected!");
    connectStatus_->setStyleSheet("color: #27ae60;");
    LOG_INFO("Remote connection established!");

    // 保存到设备列表
    saveCurrentConnection();

    viewerWidget_->setSession(viewerSession_);
    connect(viewerSession_, &rd::ViewerSession::frameReady, viewerWidget_,
            &ViewerWidget::setFrame);
    connect(viewerSession_, &rd::ViewerSession::disconnected, this,
            &MyDeskWindow::onViewerDisconnected);

    // 设置文件传输和音频通道
    // fileTransferMgr_->setChannel(viewerSession_->channel());
    // audioMgr_->setChannel(viewerSession_->channel());

    viewerSession_->startReceiving();

    stack_->setCurrentIndex(1);
    viewerWidget_->setFocus();
    setWindowTitle("myDesk - Remote Controlling");
}

void MyDeskWindow::onViewerFailed(const QString& msg) {
    connectStatus_->setText(msg);
    connectStatus_->setStyleSheet("color: #e74c3c;");
    connectBtn_->setEnabled(true);
    LOG_ERROR(QString("Connection failed: %1").arg(msg));
}

void MyDeskWindow::onViewerDisconnected() {
    if (viewerSession_) {
        viewerSession_->stop();
        delete viewerSession_;
        viewerSession_ = nullptr;
    }
    stack_->setCurrentIndex(0);
    connectBtn_->setEnabled(true);
    connectStatus_->setText("Disconnected");
    connectStatus_->setStyleSheet("color: #e74c3c;");
    setWindowTitle("myDesk - Remote Desktop");
    LOG_INFO("Disconnected from remote");
}

void MyDeskWindow::onRefreshPassword() {
    identity_.regeneratePassword();
    passwordLabel_->setText(QString::fromStdString(identity_.password));
    hostService_->setPassword(identity_.password);
    LOG_INFO("Password refreshed");
}

void MyDeskWindow::onP2PRegisterClicked() {
    const std::string sigHost = signalHostEdit_->text().toStdString();
    const uint16_t sigPort =
        static_cast<uint16_t>(signalPortEdit_->text().toUShort());

    if (sigHost.empty() || sigPort == 0) {
        p2pStatusLabel_->setText("● Please enter signal server address");
        p2pStatusLabel_->setStyleSheet("color: #e74c3c;");
        return;
    }

    p2pRegisterBtn_->setEnabled(false);
    p2pStatusLabel_->setText("● Registering...");
    p2pStatusLabel_->setStyleSheet("color: #f39c12;");
    LOG_INFO(QString("Registering P2P with %1:%2").arg(
        QString::fromStdString(sigHost)).arg(sigPort));

    hostService_->registerP2P(sigHost, sigPort, identity_.deviceId);
}

void MyDeskWindow::onQuickConnect(const std::string& deviceId) {
    const rd::SavedDevice* dev = deviceListMgr_->findDevice(deviceId);
    if (!dev) return;

    LOG_INFO(QString("Quick connect to: %1").arg(QString::fromStdString(deviceId)));

    // 填充连接信息
    if (dev->connectionMode == 0) {
        modeBox_->setCurrentIndex(0);
        addressEdit_->setText(QString::fromStdString(dev->lastAddress));
        portEdit_->setText(QString::number(dev->lastPort));
    } else {
        modeBox_->setCurrentIndex(1);
        remoteIdEdit_->setText(QString::fromStdString(dev->deviceId));
    }

    if (dev->savePassword && !dev->lastPassword.empty()) {
        remotePasswordEdit_->setText(QString::fromStdString(dev->lastPassword));
    }

    // 自动连接
    onConnectClicked();
}

void MyDeskWindow::onModeChanged(int mode) {
    rd::UserMode umode = static_cast<rd::UserMode>(mode);
    accountMgr_->setUserMode(umode);
    updateUIForMode(umode);
    LOG_INFO(QString("Switched to %1 mode").arg(
        mode == 0 ? "Developer" : "User"));
}

void MyDeskWindow::onAudioToggle() {
    if (audioMgr_->isCapturing()) {
        audioMgr_->stopCapture();
        audioMgr_->stopPlayback();
        audioBtn_->setChecked(false);
        audioBtn_->setText("🎤 Voice Call");
        audioStatusLabel_->setText("Audio: Off");
        audioStatusLabel_->setStyleSheet("color: #999; font-size: 11px;");
        LOG_INFO("Voice call stopped");
    } else {
        audioMgr_->startCapture();
        audioMgr_->startPlayback();
        audioBtn_->setChecked(true);
        audioBtn_->setText("🔇 End Call");
        audioStatusLabel_->setText("Audio: Active 🔊");
        audioStatusLabel_->setStyleSheet("color: #27ae60; font-size: 11px;");
        LOG_INFO("Voice call started");
    }
}

void MyDeskWindow::onWebServerToggle() {
    if (webServer_->isRunning()) {
        webServer_->stop();
        webServerBtn_->setText("Start Web");
        webServerStatus_->setText("● Web: off");
        webServerStatus_->setStyleSheet("color: #999; font-size: 11px;");
        LOG_INFO("Web server stopped");
    } else {
        if (webServer_->start(8080)) {
            webServerBtn_->setText("Stop Web");
        } else {
            LOG_ERROR("Failed to start web server");
        }
    }
}

void MyDeskWindow::onRecordToggle() {
    if (screenRecorder_->isRecording()) {
        // 停止录制
        screenRecorder_->stop();
        recordBtn_->setChecked(false);
        recordBtn_->setText("⏺ Record Screen");
        recordStatusLabel_->setText("Recording: Off");
        recordStatusLabel_->setStyleSheet("color: #999; font-size: 11px;");
        if (recordTimer_) {
            recordTimer_->stop();
            delete recordTimer_;
            recordTimer_ = nullptr;
        }
        LOG_INFO("Screen recording stopped");
        QMessageBox::information(this, "Recording Saved",
                                 "Screen recording has been saved.");
    } else {
        // 选择保存路径
        const QString defaultDir =
            QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        const QString timestamp =
            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        const QString defaultName =
            QString("%1/mydesk_record_%2.mp4").arg(defaultDir, timestamp);

        const QString filePath = QFileDialog::getSaveFileName(
            this, "Save Recording", defaultName,
            "MP4 Video (*.mp4);;All Files (*)");

        if (filePath.isEmpty()) {
            recordBtn_->setChecked(false);
            return;
        }

        // 开始录制
        screenRecorder_->setErrorCallback([this](const std::string& err) {
            QMetaObject::invokeMethod(this, [this, err]() {
                LOG_ERROR(QString("Recording error: %1").arg(
                    QString::fromStdString(err)));
                recordBtn_->setChecked(false);
                recordBtn_->setText("⏺ Record Screen");
                recordStatusLabel_->setText("Recording: Error");
                recordStatusLabel_->setStyleSheet("color: #e74c3c; font-size: 11px;");
                if (recordTimer_) {
                    recordTimer_->stop();
                    delete recordTimer_;
                    recordTimer_ = nullptr;
                }
            });
        });

        rd::ScreenRecorder::Config cfg;
        cfg.fps = 30;
        cfg.bitrateKbps = 8000;

        if (screenRecorder_->start(filePath.toStdString(), cfg)) {
            recordBtn_->setChecked(true);
            recordBtn_->setText("⏹ Stop Recording");
            recordStatusLabel_->setText("Recording: 00:00");
            recordStatusLabel_->setStyleSheet("color: #e74c3c; font-size: 11px; font-weight: bold;");
            LOG_INFO(QString("Screen recording started: %1").arg(filePath));

            // 定时器更新录制时长显示
            recordTimer_ = new QTimer(this);
            connect(recordTimer_, &QTimer::timeout, this, [this]() {
                if (!screenRecorder_->isRecording()) return;
                const double secs = screenRecorder_->durationSeconds();
                const int mins = static_cast<int>(secs) / 60;
                const int s = static_cast<int>(secs) % 60;
                recordStatusLabel_->setText(
                    QString("Recording: %1:%2")
                        .arg(mins, 2, 10, QChar('0'))
                        .arg(s, 2, 10, QChar('0')));
            });
            recordTimer_->start(1000);
        } else {
            recordBtn_->setChecked(false);
            LOG_ERROR("Failed to start screen recording");
        }
    }
}

void MyDeskWindow::updateUIForMode(rd::UserMode mode) {
    bool isDev = (mode == rd::UserMode::Developer);

    // 开发者模式显示日志面板和详细信息
    bottomTabs_->setVisible(isDev);
    p2pErrorEdit_->setVisible(isDev && p2pErrorEdit_->isVisible());

    // 用户模式下隐藏高级选项
    if (signalHostEdit_) signalHostEdit_->setVisible(isDev);
    if (signalPortEdit_) signalPortEdit_->setVisible(isDev);
    if (viewerSignalHostEdit_) viewerSignalHostEdit_->setVisible(isDev);
    if (viewerSignalPortEdit_) viewerSignalPortEdit_->setVisible(isDev);
}

void MyDeskWindow::saveCurrentConnection() {
    rd::SavedDevice dev;
    if (modeBox_->currentIndex() == 0) {
        dev.deviceId = addressEdit_->text().toStdString();
        dev.lastAddress = addressEdit_->text().toStdString();
        dev.lastPort = static_cast<uint16_t>(portEdit_->text().toUShort());
        dev.connectionMode = 0;
    } else {
        dev.deviceId = remoteIdEdit_->text().toStdString();
        dev.connectionMode = 1;
    }
    dev.savePassword = false;  // 默认不保存密码

    deviceListMgr_->addDevice(dev);
    deviceListMgr_->markConnected(dev.deviceId);

    // 刷新列表
    deviceListWidget_->clear();
    for (const auto& d : deviceListMgr_->devices()) {
        QString label = QString::fromStdString(d.alias.empty() ? d.deviceId : d.alias);
        if (!d.lastAddress.empty()) {
            label += QString(" (%1)").arg(QString::fromStdString(d.lastAddress));
        }
        deviceListWidget_->addItem(label);
    }
}
