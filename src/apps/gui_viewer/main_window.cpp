#include "main_window.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "core/net/tcp_socket.h"
#include "remote_session.h"
#include "video_widget.h"

#if defined(RD_HAVE_P2P)
#include "core/p2p/rtc_transport.h"
#endif

MainWindow::MainWindow() {
    setWindowTitle("todesk viewer");
    resize(960, 600);

    stack_ = new QStackedWidget(this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(stack_);

    stack_->addWidget(buildConnectPage());

    video_ = new VideoWidget;
    stack_->addWidget(video_);

    session_ = new rd::RemoteSession(this);
    video_->setSession(session_);

    connect(session_, &rd::RemoteSession::frameReady, video_,
            &VideoWidget::setFrame);
    connect(session_, &rd::RemoteSession::disconnected, this,
            &MainWindow::onDisconnected);

    connect(this, &MainWindow::established, this, &MainWindow::onEstablished);
    connect(this, &MainWindow::failed, this, &MainWindow::onFailed);
}

MainWindow::~MainWindow() {
    if (connectThread_.joinable()) connectThread_.join();
    if (session_) session_->stop();
}

QWidget* MainWindow::buildConnectPage() {
    auto* page = new QWidget;
    auto* form = new QFormLayout(page);

    modeBox_ = new QComboBox;
    modeBox_->addItem("LAN direct");
    modeBox_->addItem("Public P2P");

    addrEdit_ = new QLineEdit("127.0.0.1");
    portEdit_ = new QLineEdit("9000");
    idEdit_ = new QLineEdit("ROOM1");
    connectBtn_ = new QPushButton("Connect");
    status_ = new QLabel(" ");

    form->addRow("Mode", modeBox_);
    form->addRow("Address", addrEdit_);
    form->addRow("Port", portEdit_);
    form->addRow("Peer ID", idEdit_);
    form->addRow(connectBtn_);
    form->addRow(status_);

    connect(modeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                idEdit_->setEnabled(idx == 1);
                portEdit_->setText(idx == 1 ? "7000" : "9000");
            });
    idEdit_->setEnabled(false);

    connect(connectBtn_, &QPushButton::clicked, this,
            &MainWindow::onConnectClicked);
    return page;
}

void MainWindow::onConnectClicked() {
    if (connectThread_.joinable()) connectThread_.join();

    const bool p2p = modeBox_->currentIndex() == 1;
    const std::string addr = addrEdit_->text().toStdString();
    const uint16_t port =
        static_cast<uint16_t>(portEdit_->text().toUShort());
    const std::string id = idEdit_->text().toStdString();

    connectBtn_->setEnabled(false);
    status_->setText("Connecting...");

    connectThread_ = std::thread([this, p2p, addr, port, id]() {
        std::unique_ptr<rd::IChannel> ch;
        if (p2p) {
#if defined(RD_HAVE_P2P)
            auto rtc = std::make_unique<rd::RtcTransport>();
            if (rtc->startViewer(addr, port, id) &&
                rtc->waitConnected(30000)) {
                ch = std::move(rtc);
            }
#endif
        } else {
            auto conn = rd::net::TcpConn::connect(addr, port);
            if (conn.valid()) {
                ch = std::make_unique<rd::TcpChannel>(std::move(conn));
            }
        }

        if (ch) {
            pendingChannel_ = std::move(ch);
            emit established();
        } else {
            emit failed("Connection failed");
        }
    });
}

void MainWindow::onEstablished() {
    session_->start(std::move(pendingChannel_));
    stack_->setCurrentIndex(1);
    video_->setFocus();
    setWindowTitle("todesk viewer - connected");
}

void MainWindow::onFailed(const QString& msg) {
    status_->setText(msg);
    connectBtn_->setEnabled(true);
}

void MainWindow::onDisconnected() {
    setWindowTitle("todesk viewer - disconnected");
    status_->setText("Connection closed");
    connectBtn_->setEnabled(true);
    stack_->setCurrentIndex(0);
}
