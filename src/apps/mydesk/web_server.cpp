#include "web_server.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>

#include <cstring>

namespace rd {

WebServer::WebServer(QObject* parent) : QObject(parent) {
    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &WebServer::onNewConnection);
}

WebServer::~WebServer() {
    stop();
}

bool WebServer::start(uint16_t port) {
    if (running_.load()) return true;

    port_ = port;
    if (!server_->listen(QHostAddress::Any, port)) {
        emit error(QString("Cannot start web server on port %1: %2")
                       .arg(port).arg(server_->errorString()));
        return false;
    }

    running_.store(true);
    emit started(port);
    return true;
}

void WebServer::stop() {
    if (!running_.load()) return;
    running_.store(false);
    server_->close();

    // 关闭所有客户端
    for (auto& [socket, state] : clients_) {
        socket->close();
    }
    clients_.clear();

    emit stopped();
}

void WebServer::onNewConnection() {
    while (server_->hasPendingConnections()) {
        QTcpSocket* client = server_->nextPendingConnection();
        clients_[client] = ClientState{};

        connect(client, &QTcpSocket::readyRead, this, &WebServer::onClientData);
        connect(client, &QTcpSocket::disconnected, this, &WebServer::onClientDisconnected);

        emit clientConnected(client->peerAddress().toString());
    }
}

void WebServer::onClientData() {
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    auto it = clients_.find(client);
    if (it == clients_.end()) return;

    QByteArray data = client->readAll();

    if (it->second.isWebSocket) {
        it->second.buffer.append(data);
        handleWebSocketFrame(client, it->second.buffer);
    } else {
        it->second.buffer.append(data);
        // 检查 HTTP 请求是否完整
        if (it->second.buffer.contains("\r\n\r\n")) {
            handleHttpRequest(client, it->second.buffer);
            it->second.buffer.clear();
        }
    }
}

void WebServer::onClientDisconnected() {
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    emit clientDisconnected(client->peerAddress().toString());
    clients_.erase(client);
    client->deleteLater();
}

void WebServer::handleHttpRequest(QTcpSocket* client, const QByteArray& data) {
    QString request = QString::fromUtf8(data);
    QStringList lines = request.split("\r\n");
    if (lines.isEmpty()) return;

    QStringList firstLine = lines[0].split(" ");
    if (firstLine.size() < 2) return;

    QString method = firstLine[0];
    QString path = firstLine[1];

    // 检查是否是 WebSocket 升级请求
    if (request.contains("Upgrade: websocket", Qt::CaseInsensitive)) {
        upgradeToWebSocket(client, data);
        return;
    }

    // HTTP 路由
    if (method == "GET") {
        if (path == "/" || path == "/index.html") {
            sendHttpResponse(client, 200, "text/html", getIndexHtml());
        } else if (path == "/viewer.js") {
            sendHttpResponse(client, 200, "application/javascript", getViewerJs());
        } else if (path == "/style.css") {
            sendHttpResponse(client, 200, "text/css", getStyleCss());
        } else if (path == "/api/status") {
            QJsonObject status;
            status["running"] = true;
            status["version"] = "1.0.0";
            status["clients"] = static_cast<int>(clients_.size());
            QByteArray json = QJsonDocument(status).toJson();
            sendHttpResponse(client, 200, "application/json", json);
        } else {
            sendHttpResponse(client, 404, "text/plain", "Not Found");
        }
    } else if (method == "POST") {
        if (path == "/api/auth") {
            // 认证请求
            // 从 body 中提取密码
            int bodyStart = data.indexOf("\r\n\r\n") + 4;
            QByteArray body = data.mid(bodyStart);
            QJsonDocument doc = QJsonDocument::fromJson(body);
            QString pwd = doc.object()["password"].toString();

            QJsonObject resp;
            if (pwd.toStdString() == password_ || password_.empty()) {
                resp["success"] = true;
                resp["token"] = "web_session_token";
            } else {
                resp["success"] = false;
                resp["error"] = "Invalid password";
            }
            sendHttpResponse(client, 200, "application/json",
                             QJsonDocument(resp).toJson());
        } else {
            sendHttpResponse(client, 404, "text/plain", "Not Found");
        }
    }
}

void WebServer::sendHttpResponse(QTcpSocket* client, int statusCode,
                                 const QString& contentType, const QByteArray& body) {
    QString statusText;
    switch (statusCode) {
    case 200: statusText = "OK"; break;
    case 404: statusText = "Not Found"; break;
    case 401: statusText = "Unauthorized"; break;
    default: statusText = "Error"; break;
    }

    QByteArray response;
    response.append(QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    response.append(QString("Content-Type: %1; charset=utf-8\r\n").arg(contentType).toUtf8());
    response.append(QString("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    response.append("Access-Control-Allow-Origin: *\r\n");
    response.append("Connection: close\r\n");
    response.append("\r\n");
    response.append(body);

    client->write(response);
    client->flush();

    // 非 WebSocket 连接在发送后关闭
    auto it = clients_.find(client);
    if (it != clients_.end() && !it->second.isWebSocket) {
        client->disconnectFromHost();
    }
}

void WebServer::upgradeToWebSocket(QTcpSocket* client, const QByteArray& request) {
    // 提取 Sec-WebSocket-Key
    QString reqStr = QString::fromUtf8(request);
    QRegularExpression rx("Sec-WebSocket-Key:\\s*(\\S+)");
    QRegularExpressionMatch match = rx.match(reqStr);
    if (!match.hasMatch()) {
        sendHttpResponse(client, 400, "text/plain", "Bad Request");
        return;
    }
    QString key = match.captured(1);

    // 计算 accept key
    QString magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    QByteArray acceptKey = QCryptographicHash::hash(
        (key + magic).toUtf8(), QCryptographicHash::Sha1).toBase64();

    // 发送升级响应
    QByteArray response;
    response.append("HTTP/1.1 101 Switching Protocols\r\n");
    response.append("Upgrade: websocket\r\n");
    response.append("Connection: Upgrade\r\n");
    response.append(QString("Sec-WebSocket-Accept: %1\r\n").arg(QString::fromUtf8(acceptKey)).toUtf8());
    response.append("\r\n");

    client->write(response);
    client->flush();

    auto it = clients_.find(client);
    if (it != clients_.end()) {
        it->second.isWebSocket = true;
        it->second.buffer.clear();
    }
}

void WebServer::handleWebSocketFrame(QTcpSocket* client, const QByteArray& data) {
    // 简化的 WebSocket 帧解析
    if (data.size() < 2) return;

    uint8_t firstByte = static_cast<uint8_t>(data[0]);
    uint8_t secondByte = static_cast<uint8_t>(data[1]);

    // bool fin = (firstByte & 0x80) != 0;
    uint8_t opcode = firstByte & 0x0F;
    bool masked = (secondByte & 0x80) != 0;
    uint64_t payloadLen = secondByte & 0x7F;

    int offset = 2;
    if (payloadLen == 126) {
        if (data.size() < 4) return;
        payloadLen = (static_cast<uint8_t>(data[2]) << 8) | static_cast<uint8_t>(data[3]);
        offset = 4;
    } else if (payloadLen == 127) {
        if (data.size() < 10) return;
        payloadLen = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLen = (payloadLen << 8) | static_cast<uint8_t>(data[2 + i]);
        }
        offset = 10;
    }

    uint8_t mask[4] = {0, 0, 0, 0};
    if (masked) {
        if (data.size() < offset + 4) return;
        for (int i = 0; i < 4; ++i) {
            mask[i] = static_cast<uint8_t>(data[offset + i]);
        }
        offset += 4;
    }

    if (static_cast<uint64_t>(data.size() - offset) < payloadLen) return;

    QByteArray payload = data.mid(offset, static_cast<int>(payloadLen));
    if (masked) {
        for (int i = 0; i < payload.size(); ++i) {
            payload[i] = payload[i] ^ mask[i % 4];
        }
    }

    // 清除已处理的数据
    auto it = clients_.find(client);
    if (it != clients_.end()) {
        it->second.buffer = data.mid(offset + static_cast<int>(payloadLen));
    }

    switch (opcode) {
    case 0x01: {  // Text frame
        // 处理 JSON 命令（鼠标/键盘事件、认证等）
        QJsonDocument doc = QJsonDocument::fromJson(payload);
        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();

        if (type == "auth") {
            QString pwd = obj["password"].toString();
            bool ok = (pwd.toStdString() == password_) || password_.empty();
            if (it != clients_.end()) it->second.authenticated = ok;

            QJsonObject resp;
            resp["type"] = "auth_result";
            resp["success"] = ok;
            sendWebSocketFrame(client, QJsonDocument(resp).toJson(QJsonDocument::Compact));
        } else if (type == "input") {
            // 鼠标/键盘输入事件
            // TODO: 转发给 HostService 的 InputInjector
            // obj["inputType"] = "mouse_move" | "mouse_down" | "mouse_up" | "key_down" | "key_up"
            // obj["x"], obj["y"], obj["button"], obj["key"]
        }
        break;
    }
    case 0x08: {  // Close frame
        client->close();
        break;
    }
    case 0x09: {  // Ping
        sendWebSocketFrame(client, payload, true);  // Pong
        break;
    }
    default:
        break;
    }
}

void WebServer::sendWebSocketFrame(QTcpSocket* client, const QByteArray& payload, bool isBinary) {
    QByteArray frame;
    uint8_t firstByte = 0x80 | (isBinary ? 0x02 : 0x01);  // FIN + opcode
    frame.append(static_cast<char>(firstByte));

    if (payload.size() < 126) {
        frame.append(static_cast<char>(payload.size()));
    } else if (payload.size() < 65536) {
        frame.append(static_cast<char>(126));
        frame.append(static_cast<char>((payload.size() >> 8) & 0xFF));
        frame.append(static_cast<char>(payload.size() & 0xFF));
    } else {
        frame.append(static_cast<char>(127));
        uint64_t len = static_cast<uint64_t>(payload.size());
        for (int i = 7; i >= 0; --i) {
            frame.append(static_cast<char>((len >> (i * 8)) & 0xFF));
        }
    }

    frame.append(payload);
    client->write(frame);
}

// ============ 内嵌 HTML/JS/CSS ============

QByteArray WebServer::getIndexHtml() const {
    return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>myDesk - Web Remote Desktop</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <div id="app">
        <!-- Login Panel -->
        <div id="login-panel" class="panel">
            <h1>🖥️ myDesk</h1>
            <p class="subtitle">Web Remote Desktop</p>
            <div class="form-group">
                <input type="password" id="password-input" placeholder="Enter password" />
                <button id="connect-btn" onclick="doConnect()">Connect</button>
            </div>
            <p id="login-status" class="status"></p>
        </div>

        <!-- Remote Desktop Panel -->
        <div id="desktop-panel" class="panel" style="display:none;">
            <div class="toolbar">
                <span class="toolbar-title">myDesk Remote Desktop</span>
                <span id="fps-display" class="fps">FPS: --</span>
                <button onclick="toggleFullscreen()" class="toolbar-btn">⛶ Fullscreen</button>
                <button onclick="disconnect()" class="toolbar-btn danger">✕ Disconnect</button>
            </div>
            <div id="canvas-container">
                <canvas id="remote-canvas" tabindex="1"></canvas>
            </div>
            <div class="status-bar">
                <span id="connection-status">Connected</span>
                <span id="resolution-info">--</span>
            </div>
        </div>
    </div>
    <script src="/viewer.js"></script>
</body>
</html>
)HTML";
}

QByteArray WebServer::getViewerJs() const {
    return R"JS(
// myDesk Web Viewer - WebSocket based remote desktop client

let ws = null;
let canvas = null;
let ctx = null;
let authenticated = false;

function doConnect() {
    const password = document.getElementById('password-input').value;
    const statusEl = document.getElementById('login-status');

    statusEl.textContent = 'Connecting...';
    statusEl.className = 'status connecting';

    // Connect WebSocket
    const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    ws = new WebSocket(`${protocol}//${location.host}/ws`);

    ws.onopen = () => {
        // Send authentication
        ws.send(JSON.stringify({ type: 'auth', password: password }));
    };

    ws.onmessage = (event) => {
        if (typeof event.data === 'string') {
            const msg = JSON.parse(event.data);
            handleMessage(msg);
        } else {
            // Binary data = video frame (JPEG)
            handleVideoFrame(event.data);
        }
    };

    ws.onerror = () => {
        statusEl.textContent = 'Connection failed';
        statusEl.className = 'status error';
    };

    ws.onclose = () => {
        if (authenticated) {
            disconnect();
        }
        statusEl.textContent = 'Disconnected';
        statusEl.className = 'status error';
    };
}

function handleMessage(msg) {
    switch (msg.type) {
        case 'auth_result':
            if (msg.success) {
                authenticated = true;
                showDesktop();
            } else {
                document.getElementById('login-status').textContent = 'Authentication failed';
                document.getElementById('login-status').className = 'status error';
                ws.close();
            }
            break;
        case 'resolution':
            document.getElementById('resolution-info').textContent =
                `${msg.width}x${msg.height}`;
            canvas.width = msg.width;
            canvas.height = msg.height;
            break;
    }
}

function handleVideoFrame(blob) {
    const img = new Image();
    img.onload = () => {
        if (canvas && ctx) {
            ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
        }
        URL.revokeObjectURL(img.src);
    };
    img.src = URL.createObjectURL(blob);
}

function showDesktop() {
    document.getElementById('login-panel').style.display = 'none';
    document.getElementById('desktop-panel').style.display = 'block';

    canvas = document.getElementById('remote-canvas');
    ctx = canvas.getContext('2d');
    canvas.focus();

    // Setup input event handlers
    setupInputHandlers();
}

function setupInputHandlers() {
    // Mouse move
    canvas.addEventListener('mousemove', (e) => {
        if (!authenticated) return;
        const rect = canvas.getBoundingClientRect();
        const x = (e.clientX - rect.left) / rect.width;
        const y = (e.clientY - rect.top) / rect.height;
        ws.send(JSON.stringify({
            type: 'input', inputType: 'mouse_move',
            x: x, y: y
        }));
    });

    // Mouse buttons
    canvas.addEventListener('mousedown', (e) => {
        if (!authenticated) return;
        e.preventDefault();
        const rect = canvas.getBoundingClientRect();
        ws.send(JSON.stringify({
            type: 'input', inputType: 'mouse_down',
            x: (e.clientX - rect.left) / rect.width,
            y: (e.clientY - rect.top) / rect.height,
            button: e.button
        }));
    });

    canvas.addEventListener('mouseup', (e) => {
        if (!authenticated) return;
        e.preventDefault();
        const rect = canvas.getBoundingClientRect();
        ws.send(JSON.stringify({
            type: 'input', inputType: 'mouse_up',
            x: (e.clientX - rect.left) / rect.width,
            y: (e.clientY - rect.top) / rect.height,
            button: e.button
        }));
    });

    // Scroll wheel
    canvas.addEventListener('wheel', (e) => {
        if (!authenticated) return;
        e.preventDefault();
        ws.send(JSON.stringify({
            type: 'input', inputType: 'wheel',
            deltaX: e.deltaX, deltaY: e.deltaY
        }));
    });

    // Keyboard
    canvas.addEventListener('keydown', (e) => {
        if (!authenticated) return;
        e.preventDefault();
        ws.send(JSON.stringify({
            type: 'input', inputType: 'key_down',
            key: e.key, code: e.code, keyCode: e.keyCode
        }));
    });

    canvas.addEventListener('keyup', (e) => {
        if (!authenticated) return;
        e.preventDefault();
        ws.send(JSON.stringify({
            type: 'input', inputType: 'key_up',
            key: e.key, code: e.code, keyCode: e.keyCode
        }));
    });

    // Prevent context menu
    canvas.addEventListener('contextmenu', (e) => e.preventDefault());
}

function toggleFullscreen() {
    const container = document.getElementById('canvas-container');
    if (document.fullscreenElement) {
        document.exitFullscreen();
    } else {
        container.requestFullscreen();
    }
}

function disconnect() {
    authenticated = false;
    if (ws) {
        ws.close();
        ws = null;
    }
    document.getElementById('login-panel').style.display = 'block';
    document.getElementById('desktop-panel').style.display = 'none';
}
)JS";
}

QByteArray WebServer::getStyleCss() const {
    return R"CSS(
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: #1a1a2e;
    color: #eee;
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
}
#app { width: 100%; height: 100vh; display: flex; align-items: center; justify-content: center; }

.panel { width: 100%; max-width: 500px; padding: 40px; text-align: center; }
#desktop-panel { max-width: 100%; padding: 0; height: 100vh; display: flex; flex-direction: column; }

h1 { font-size: 2.5em; margin-bottom: 5px; }
.subtitle { color: #888; margin-bottom: 30px; }

.form-group { display: flex; gap: 10px; justify-content: center; margin: 20px 0; }
.form-group input {
    padding: 12px 16px; border: 1px solid #333; border-radius: 8px;
    background: #16213e; color: #eee; font-size: 16px; width: 250px;
}
.form-group input:focus { outline: none; border-color: #3498db; }
.form-group button {
    padding: 12px 24px; background: #3498db; color: white; border: none;
    border-radius: 8px; font-size: 16px; cursor: pointer; font-weight: bold;
}
.form-group button:hover { background: #2980b9; }

.status { margin-top: 10px; font-size: 14px; }
.status.connecting { color: #f39c12; }
.status.error { color: #e74c3c; }

.toolbar {
    display: flex; align-items: center; gap: 15px;
    padding: 8px 15px; background: #16213e; border-bottom: 1px solid #333;
}
.toolbar-title { font-weight: bold; flex: 1; }
.fps { color: #27ae60; font-size: 12px; font-family: monospace; }
.toolbar-btn {
    padding: 6px 12px; background: #2c3e50; color: white; border: none;
    border-radius: 4px; cursor: pointer; font-size: 13px;
}
.toolbar-btn:hover { background: #34495e; }
.toolbar-btn.danger { background: #e74c3c; }
.toolbar-btn.danger:hover { background: #c0392b; }

#canvas-container { flex: 1; overflow: hidden; background: #000; display: flex; align-items: center; justify-content: center; }
#remote-canvas { max-width: 100%; max-height: 100%; cursor: none; }

.status-bar {
    display: flex; justify-content: space-between; padding: 5px 15px;
    background: #16213e; border-top: 1px solid #333; font-size: 12px; color: #888;
}
)CSS";
}

}  // namespace rd
