// myDesk unified client entry point.
// Like ToDesk: launches as both host (background listener) and viewer (connect to others).
// Same program, bidirectional control, multi-platform support.
//
// 运行模式:
//   GUI 模式 (默认): 启动图形界面，同时作为被控端和控制端
//   Headless 模式:   mydesk --headless [--port 9000]
//                    无界面纯服务，适合部署在服务器上

#include <QApplication>
#include <QCommandLineParser>
#include <QScreen>

#include <cstdio>
#include <cstring>

#include "core/net/tcp_socket.h"
#include "headless_service.h"
#include "mydesk_window.h"

#ifdef _WIN32
#include <windows.h>
#endif

// 隐藏 Windows 下的控制台窗口（GUI 模式时）
static void hideConsoleWindow() {
#ifdef _WIN32
    // 如果是从资源管理器等双击启动（有自己的控制台），隐藏它
    HWND console = GetConsoleWindow();
    if (console) {
        // 检查是否是我们自己创建的控制台（父进程是 explorer 时通常是独立控制台）
        DWORD consoleProcessId = 0;
        GetWindowThreadProcessId(console, &consoleProcessId);
        if (consoleProcessId == GetCurrentProcessId()) {
            FreeConsole();
        } else {
            // 由命令行启动时不隐藏，允许日志输出
            // 但如果用户双击 exe，则隐藏
            ShowWindow(console, SW_HIDE);
        }
    }
#endif
}

int main(int argc, char** argv) {
    rd::net::initSockets();

    // 快速预扫描参数判断是否 headless 模式（避免初始化 QApplication）
    bool headless = false;
    uint16_t port = 9000;
    std::string password;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--headless") == 0 ||
            std::strcmp(argv[i], "-H") == 0) {
            headless = true;
        } else if ((std::strcmp(argv[i], "--port") == 0 ||
                    std::strcmp(argv[i], "-p") == 0) &&
                   i + 1 < argc) {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--password") == 0 && i + 1 < argc) {
            password = argv[++i];
        }
    }

    if (headless) {
        // Headless 模式: 纯服务端，无 GUI，适合 Linux 服务器部署
        std::printf("myDesk headless service starting on port %u...\n", port);
        rd::HeadlessService svc;
        if (!password.empty()) svc.setPassword(password);
        return svc.run(port);
    }

    // GUI 模式: 隐藏控制台窗口
    hideConsoleWindow();

    // HiDPI 缩放必须在 QApplication 创建之前设置
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName("myDesk");
    app.setOrganizationName("myDesk");
    app.setApplicationVersion("1.0.0");

    // 根据屏幕 DPI 计算基准字号，确保在各种分辨率下清晰可读
    int baseFontSize = 14;
    if (auto* screen = app.primaryScreen()) {
        const qreal dpi = screen->logicalDotsPerInch();
        if (dpi >= 144) baseFontSize = 16;  // 150% 缩放
        else if (dpi >= 120) baseFontSize = 15;  // 125% 缩放
    }

    app.setStyleSheet(QString(R"(
        * {
            font-size: %1px;
        }
        QGroupBox {
            border: 1px solid #dfe6e9;
            border-radius: 8px;
            margin-top: 12px;
            padding-top: 18px;
            font-weight: bold;
            font-size: %2px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 5px;
        }
        QLineEdit {
            border: 1px solid #dfe6e9;
            border-radius: 4px;
            padding: 10px;
            font-size: %1px;
        }
        QLineEdit:focus {
            border-color: #3498db;
        }
        QComboBox {
            border: 1px solid #dfe6e9;
            border-radius: 4px;
            padding: 8px;
            font-size: %1px;
        }
        QLabel {
            font-size: %1px;
        }
        QPushButton {
            font-size: %1px;
        }
    )")
                          .arg(baseFontSize)
                          .arg(baseFontSize + 1));

    MyDeskWindow w;
    w.show();
    const int rc = app.exec();

    rd::net::shutdownSockets();
    return rc;
}
