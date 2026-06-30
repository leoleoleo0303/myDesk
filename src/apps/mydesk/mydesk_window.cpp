#include "mydesk_window.h"

#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "host_service.h"
#include "viewer_session.h"
#include "viewer_widget.h"

MyDeskWindow::MyDeskWindow() {
    setWindowTitle("myDesk - Remote Desktop");
    resize(1060, 680);
    setMinimumSize(900, 600);

    identity_ = rd::DeviceIdentity::load();

    stack_ = new QStackedWidget;
    setCentralWidget(stack_);

    stack_->addWidget(buildMainPage());

    viewerWidget_ = new ViewerWidget;
    stack_->addWidget(viewerWidget_);

    // Start host service - LAN listener (direct TCP on port 9000)
    hostService_ = new rd::HostService(this);
    hostService_->setPassword(identity_.password);
    hostService_->startListening(9000);

    connect(hostService_, &rd::HostService::clientConnected, this,
            [this](const QString&) {
                hostStatusLabel_->setText("● Someone is controlling this PC");
                hostStatusLabel_->setStyleSheet("color: #e74c3c; font-weight: bold;");
            });
    connect(hostService_, &rd::HostService::clientDisconnected, this,
            [this]() {
                hostStatusLabel_->setText("● Waiting for connection...");
                hostStatusLabel_->setStyleSheet("color: #27ae60;");
            });
    connect(hostService_, &rd::HostService::p2pRegistered, this,
            [this]() {
                p2pStatusLabel_->setText("● P2P registered (ready)");
                p2pStatusLabel_->setStyleSheet("color: #27ae60;");
                p2pErrorEdit_->setVisible(false);
                p2pRegisterBtn_->setText("Re-register");
                p2pRegisterBtn_->setEnabled(true);
            });
    connect(hostService_, &rd::HostService::p2pRegisterFailed, this,
            [this](const QString& msg) {
                p2pStatusLabel_->setText("● P2P: " + msg);
                p2pStatusLabel_->setStyleSheet("color: #e74c3c;");
                p2pStatusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
                p2pErrorEdit_->setText(msg);
                p2pErrorEdit_->setVisible(true);
                p2pRegisterBtn_->setEnabled(true);
            });

    connect(this, &MyDeskWindow::viewerConnected, this,
            &MyDeskWindow::onViewerConnected);
    connect(this, &MyDeskWindow::viewerConnectFailed, this,
            &MyDeskWindow::onViewerFailed);
}

MyDeskWindow::~MyDeskWindow() {
    if (connectThread_.joinable()) connectThread_.join();
    if (viewerSession_) viewerSession_->stop();
    hostService_->stop();
}

QWidget* MyDeskWindow::buildMainPage() {
    auto* page = new QWidget;
    auto* mainLayout = new QHBoxLayout(page);
    mainLayout->setSpacing(30);
    mainLayout->setContentsMargins(30, 20, 30, 25);

    mainLayout->addWidget(buildLeftPanel(), 1);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(sep);

    mainLayout->addWidget(buildRightPanel(), 1);
    return page;
}

QWidget* MyDeskWindow::buildLeftPanel() {
    auto* group = new QGroupBox("This Device");
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(15);

    auto* idTitle = new QLabel("Device ID");
    idTitle->setStyleSheet("font-size: 12px; color: #666;");
    layout->addWidget(idTitle);

    deviceIdLabel_ = new QLabel(QString::fromStdString(identity_.deviceId));
    QFont idFont;
    idFont.setPointSize(28);
    idFont.setBold(true);
    deviceIdLabel_->setFont(idFont);
    deviceIdLabel_->setStyleSheet("color: #2c3e50; letter-spacing: 3px;");
    deviceIdLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(deviceIdLabel_);

    layout->addSpacing(10);

    auto* pwdTitle = new QLabel("Temporary Password");
    pwdTitle->setStyleSheet("font-size: 12px; color: #666;");
    layout->addWidget(pwdTitle);

    auto* pwdRow = new QHBoxLayout;
    passwordLabel_ = new QLabel(QString::fromStdString(identity_.password));
    QFont pwdFont;
    pwdFont.setPointSize(22);
    pwdFont.setBold(true);
    passwordLabel_->setFont(pwdFont);
    passwordLabel_->setStyleSheet("color: #2980b9;");
    passwordLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pwdRow->addWidget(passwordLabel_);

    refreshPwdBtn_ = new QPushButton("Refresh");
    refreshPwdBtn_->setFixedHeight(30);
    refreshPwdBtn_->setToolTip("Regenerate password");
    connect(refreshPwdBtn_, &QPushButton::clicked, this,
            &MyDeskWindow::onRefreshPassword);
    pwdRow->addWidget(refreshPwdBtn_);
    pwdRow->addStretch();
    layout->addLayout(pwdRow);

    layout->addSpacing(10);

    // LAN status
    hostStatusLabel_ = new QLabel("● Waiting for connection...");
    hostStatusLabel_->setStyleSheet("color: #27ae60;");
    layout->addWidget(hostStatusLabel_);

    layout->addSpacing(15);

    // --- P2P Registration Section ---
    auto* p2pTitle = new QLabel("P2P (Signal Server)");
    p2pTitle->setStyleSheet("font-size: 12px; color: #666; font-weight: bold;");
    layout->addWidget(p2pTitle);

    auto* sigRow = new QHBoxLayout;
    signalHostEdit_ = new QLineEdit("127.0.0.1");
    signalHostEdit_->setPlaceholderText("Signal server IP");
    signalHostEdit_->setFixedWidth(150);
    sigRow->addWidget(new QLabel("Server:"));
    sigRow->addWidget(signalHostEdit_);
    signalPortEdit_ = new QLineEdit("7000");
    signalPortEdit_->setPlaceholderText("Port");
    signalPortEdit_->setFixedWidth(70);
    sigRow->addWidget(new QLabel(":"));
    sigRow->addWidget(signalPortEdit_);
    sigRow->addStretch();
    layout->addLayout(sigRow);

    p2pRegisterBtn_ = new QPushButton("Register to Signal Server");
    p2pRegisterBtn_->setFixedHeight(32);
    p2pRegisterBtn_->setStyleSheet(
        "QPushButton { background-color: #9b59b6; color: white; "
        "border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #8e44ad; }"
        "QPushButton:disabled { background-color: #bdc3c7; }");
    connect(p2pRegisterBtn_, &QPushButton::clicked, this,
            &MyDeskWindow::onP2PRegisterClicked);
    layout->addWidget(p2pRegisterBtn_);

    p2pStatusLabel_ = new QLabel("● P2P not registered");
    p2pStatusLabel_->setStyleSheet("color: #999;");
    layout->addWidget(p2pStatusLabel_);

    // Copyable error text field (hidden by default, shown on error)
    p2pErrorEdit_ = new QLineEdit;
    p2pErrorEdit_->setReadOnly(true);
    p2pErrorEdit_->setVisible(false);
    p2pErrorEdit_->setStyleSheet(
        "QLineEdit { background-color: #ffeaea; color: #c0392b; "
        "border: 1px solid #e74c3c; border-radius: 3px; padding: 4px; "
        "font-size: 11px; }");
    p2pErrorEdit_->setPlaceholderText("Error details will appear here");
    layout->addWidget(p2pErrorEdit_);

    layout->addStretch();

    auto* note = new QLabel(
        "Share the Device ID and Password\nto allow remote control.\n\n"
        "LAN: Others connect directly to port 9000.\n"
        "P2P: Register above, then others\nfind you by Device ID.");
    note->setStyleSheet("color: #999; font-size: 11px;");
    note->setWordWrap(true);
    layout->addWidget(note);

    return group;
}

QWidget* MyDeskWindow::buildRightPanel() {
    auto* group = new QGroupBox("Remote Control");
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(12);

    modeBox_ = new QComboBox;
    modeBox_->addItem("LAN Direct");
    modeBox_->addItem("Device ID (P2P)");
    layout->addWidget(new QLabel("Connection Mode"));
    layout->addWidget(modeBox_);

    // --- LAN mode fields ---
    addressEdit_ = new QLineEdit("127.0.0.1");
    addressEdit_->setPlaceholderText("Remote IP address");
    layout->addWidget(new QLabel("Remote Address"));
    layout->addWidget(addressEdit_);

    portEdit_ = new QLineEdit("9000");
    portEdit_->setPlaceholderText("Port");
    layout->addWidget(new QLabel("Port"));
    layout->addWidget(portEdit_);

    // --- P2P mode fields ---
    remoteIdEdit_ = new QLineEdit;
    remoteIdEdit_->setPlaceholderText("Enter remote Device ID");
    QFont idEditFont;
    idEditFont.setPointSize(16);
    remoteIdEdit_->setFont(idEditFont);
    layout->addWidget(new QLabel("Remote Device ID"));
    layout->addWidget(remoteIdEdit_);

    viewerSignalHostEdit_ = new QLineEdit("127.0.0.1");
    viewerSignalHostEdit_->setPlaceholderText("Signal server IP");
    layout->addWidget(new QLabel("Signal Server"));
    layout->addWidget(viewerSignalHostEdit_);

    viewerSignalPortEdit_ = new QLineEdit("7000");
    viewerSignalPortEdit_->setPlaceholderText("Signal server port");
    layout->addWidget(new QLabel("Signal Port"));
    layout->addWidget(viewerSignalPortEdit_);

    // --- Common fields ---
    remotePasswordEdit_ = new QLineEdit;
    remotePasswordEdit_->setPlaceholderText("Enter remote password");
    remotePasswordEdit_->setEchoMode(QLineEdit::Password);
    layout->addWidget(new QLabel("Password"));
    layout->addWidget(remotePasswordEdit_);

    connectBtn_ = new QPushButton("Connect Remote Desktop");
    connectBtn_->setMinimumHeight(48);
    connectBtn_->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; "
        "border-radius: 6px; font-size: 15px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:disabled { background-color: #bdc3c7; }");
    connect(connectBtn_, &QPushButton::clicked, this,
            &MyDeskWindow::onConnectClicked);
    layout->addWidget(connectBtn_);

    connectStatus_ = new QLabel("");
    connectStatus_->setStyleSheet("color: #666;");
    layout->addWidget(connectStatus_);

    layout->addStretch();

    // Mode switching: show/hide relevant fields
    auto updateMode = [this](int idx) {
        const bool isLAN = (idx == 0);
        const bool isP2P = (idx == 1);
        // LAN fields
        addressEdit_->setVisible(isLAN);
        portEdit_->setVisible(isLAN);
        // P2P fields
        remoteIdEdit_->setVisible(isP2P);
        viewerSignalHostEdit_->setVisible(isP2P);
        viewerSignalPortEdit_->setVisible(isP2P);
    };
    connect(modeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, updateMode);
    updateMode(0);  // default LAN mode

    return group;
}

void MyDeskWindow::onConnectClicked() {
    if (connectThread_.joinable()) connectThread_.join();

    const int mode = modeBox_->currentIndex();
    const std::string password = remotePasswordEdit_->text().toStdString();

    connectBtn_->setEnabled(false);
    connectStatus_->setText("Connecting...");
    connectStatus_->setStyleSheet("color: #f39c12;");

    if (mode == 0) {
        // LAN Direct: address + port are the remote host's IP and port
        const std::string addr = addressEdit_->text().toStdString();
        const uint16_t port =
            static_cast<uint16_t>(portEdit_->text().toUShort());

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
        // P2P: signal server address + remote device ID
        const std::string deviceId = remoteIdEdit_->text().toStdString();
        const std::string sigHost =
            viewerSignalHostEdit_->text().toStdString();
        const uint16_t sigPort = static_cast<uint16_t>(
            viewerSignalPortEdit_->text().toUShort());

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

    viewerWidget_->setSession(viewerSession_);
    connect(viewerSession_, &rd::ViewerSession::frameReady, viewerWidget_,
            &ViewerWidget::setFrame);
    connect(viewerSession_, &rd::ViewerSession::disconnected, this,
            &MyDeskWindow::onViewerDisconnected);
    viewerSession_->startReceiving();

    stack_->setCurrentIndex(1);
    viewerWidget_->setFocus();
    setWindowTitle("myDesk - Remote Controlling");
}

void MyDeskWindow::onViewerFailed(const QString& msg) {
    connectStatus_->setText(msg);
    connectStatus_->setStyleSheet("color: #e74c3c;");
    connectBtn_->setEnabled(true);
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
}

void MyDeskWindow::onRefreshPassword() {
    identity_.regeneratePassword();
    passwordLabel_->setText(QString::fromStdString(identity_.password));
    hostService_->setPassword(identity_.password);
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

    // Register our device ID with signal_server so remote viewers can find us
    hostService_->registerP2P(sigHost, sigPort, identity_.deviceId);
}
