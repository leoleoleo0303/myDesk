#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QString>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace rd {

// HTTP Web 服务 - 提供 Web 界面进行远程桌面控制
// 通过浏览器即可控制远程电脑（WebSocket + HTML5 Canvas）
//
// 架构:
//   Browser <-- HTTP --> WebServer (serve HTML/JS/CSS)
//   Browser <-- WebSocket --> WebServer (video stream + input events)
//   WebServer <-- internal --> HostService (screen capture + input inject)
//
// Web 客户端通过 WebSocket 接收 JPEG 帧（或后续 WebCodecs H.264），
// 并发送鼠标/键盘事件。
class WebServer : public QObject {
    Q_OBJECT
public:
    explicit WebServer(QObject* parent = nullptr);
    ~WebServer() override;

    // 启动 HTTP 服务
    bool start(uint16_t port = 8080);
    void stop();
    bool isRunning() const { return running_.load(); }
    uint16_t port() const { return port_; }

    // 设置认证密码
    void setPassword(const std::string& pwd) { password_ = pwd; }

signals:
    void started(uint16_t port);
    void stopped();
    void clientConnected(const QString& addr);
    void clientDisconnected(const QString& addr);
    void error(const QString& msg);

private slots:
    void onNewConnection();
    void onClientData();
    void onClientDisconnected();

private:
    void handleHttpRequest(QTcpSocket* client, const QByteArray& data);
    void sendHttpResponse(QTcpSocket* client, int statusCode,
                          const QString& contentType, const QByteArray& body);
    void sendFile(QTcpSocket* client, const QString& path, const QString& contentType);
    QByteArray getIndexHtml() const;
    QByteArray getViewerJs() const;
    QByteArray getStyleCss() const;

    // WebSocket
    void upgradeToWebSocket(QTcpSocket* client, const QByteArray& request);
    void handleWebSocketFrame(QTcpSocket* client, const QByteArray& data);
    void sendWebSocketFrame(QTcpSocket* client, const QByteArray& payload, bool isBinary = false);

    QTcpServer* server_ = nullptr;
    std::atomic<bool> running_{false};
    uint16_t port_ = 8080;
    std::string password_;

    struct ClientState {
        bool isWebSocket = false;
        bool authenticated = false;
        QByteArray buffer;
    };
    std::unordered_map<QTcpSocket*, ClientState> clients_;
};

}  // namespace rd
