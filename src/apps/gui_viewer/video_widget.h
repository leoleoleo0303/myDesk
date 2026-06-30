#pragma once

#include <QImage>
#include <QWidget>

namespace rd {
class RemoteSession;
}

// 显示远端画面，并把鼠标/键盘事件转成 InputEvent 通过 session 回传。
class VideoWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget* parent = nullptr);

    void setSession(rd::RemoteSession* s) { session_ = s; }

public slots:
    void setFrame(const QImage& img);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;

private:
    void sendMouseButton(QMouseEvent* e, bool down);
    void sendKey(QKeyEvent* e, bool down);

    QImage frame_;
    rd::RemoteSession* session_ = nullptr;
};
