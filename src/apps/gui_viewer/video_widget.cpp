#include "video_widget.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include "core/input/input_event.h"
#include "remote_session.h"

namespace {

// Qt::Key -> X11 keysym（与 viewer 的映射保持一致）。
uint32_t qtKeyToKeysym(int key, const QString& text) {
    switch (key) {
        case Qt::Key_Return:
        case Qt::Key_Enter: return 0xff0d;
        case Qt::Key_Escape: return 0xff1b;
        case Qt::Key_Backspace: return 0xff08;
        case Qt::Key_Tab: return 0xff09;
        case Qt::Key_Delete: return 0xffff;
        case Qt::Key_Left: return 0xff51;
        case Qt::Key_Up: return 0xff52;
        case Qt::Key_Right: return 0xff53;
        case Qt::Key_Down: return 0xff54;
        case Qt::Key_Home: return 0xff50;
        case Qt::Key_End: return 0xff57;
        case Qt::Key_PageUp: return 0xff55;
        case Qt::Key_PageDown: return 0xff56;
        case Qt::Key_Shift: return 0xffe1;
        case Qt::Key_Control: return 0xffe3;
        case Qt::Key_Alt: return 0xffe9;
        case Qt::Key_Space: return 0x20;
        default: break;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        return 0xffbe + (key - Qt::Key_F1);
    }
    // 字母：统一用小写 keysym（大小写由 Shift 决定）
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return 0x61 + (key - Qt::Key_A);
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return static_cast<uint32_t>(key);  // '0'..'9' == ASCII
    }
    // 其它可打印字符用文本
    if (!text.isEmpty()) {
        const ushort u = text[0].unicode();
        if (u >= 0x20 && u <= 0x7e) return u;
    }
    return 0;
}

}  // namespace

VideoWidget::VideoWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);  // 不按键也接收移动事件
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(320, 240);
}

void VideoWidget::setFrame(const QImage& img) {
    frame_ = img;
    update();
}

void VideoWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (!frame_.isNull()) {
        p.drawImage(rect(), frame_);  // 缩放铺满窗口
    }
}

void VideoWidget::mouseMoveEvent(QMouseEvent* e) {
    if (!session_) return;
    const int w = width() > 0 ? width() : 1;
    const int h = height() > 0 ? height() : 1;
    rd::InputEvent ie;
    ie.type = rd::InputType::MouseMove;
    ie.x = static_cast<uint16_t>(static_cast<long>(e->x()) * 65535 / w);
    ie.y = static_cast<uint16_t>(static_cast<long>(e->y()) * 65535 / h);
    session_->sendInput(ie);
}

void VideoWidget::sendMouseButton(QMouseEvent* e, bool down) {
    if (!session_) return;
    uint8_t btn = 0;
    if (e->button() == Qt::LeftButton) btn = 1;
    else if (e->button() == Qt::MiddleButton) btn = 2;
    else if (e->button() == Qt::RightButton) btn = 3;
    if (btn == 0) return;
    rd::InputEvent ie;
    ie.type = rd::InputType::MouseButton;
    ie.button = btn;
    ie.down = down ? 1 : 0;
    session_->sendInput(ie);
}

void VideoWidget::mousePressEvent(QMouseEvent* e) {
    setFocus();
    sendMouseButton(e, true);
}

void VideoWidget::mouseReleaseEvent(QMouseEvent* e) { sendMouseButton(e, false); }

void VideoWidget::wheelEvent(QWheelEvent* e) {
    if (!session_) return;
    const int steps = e->angleDelta().y() / 120;
    if (steps == 0) return;
    rd::InputEvent ie;
    ie.type = rd::InputType::MouseWheel;
    ie.wheel = static_cast<int16_t>(steps);
    session_->sendInput(ie);
}

void VideoWidget::sendKey(QKeyEvent* e, bool down) {
    if (!session_ || e->isAutoRepeat()) return;
    const uint32_t ks = qtKeyToKeysym(e->key(), e->text());
    if (ks == 0) return;
    rd::InputEvent ie;
    ie.type = rd::InputType::Key;
    ie.key = ks;
    ie.down = down ? 1 : 0;
    session_->sendInput(ie);
}

void VideoWidget::keyPressEvent(QKeyEvent* e) { sendKey(e, true); }
void VideoWidget::keyReleaseEvent(QKeyEvent* e) { sendKey(e, false); }
