#pragma once

#include <QImage>
#include <QWidget>

namespace rd {
class ViewerSession;
}

// Displays remote screen and captures mouse/keyboard input for remote control.
class ViewerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ViewerWidget(QWidget* parent = nullptr);

    void setSession(rd::ViewerSession* s) { session_ = s; }

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
    rd::ViewerSession* session_ = nullptr;
};
