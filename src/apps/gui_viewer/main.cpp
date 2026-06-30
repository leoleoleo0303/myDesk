// gui_viewer: Qt5 桌面控制端。
// 连接界面填写局域网 IP 或公网信令服务器 + 设备 id，连上后实时显示远端
// 画面，并把鼠标/键盘操作回传，实现远程控制。

#include <QApplication>

#include "core/net/tcp_socket.h"
#include "main_window.h"

int main(int argc, char** argv) {
    rd::net::initSockets();

    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    const int rc = app.exec();

    rd::net::shutdownSockets();
    return rc;
}
