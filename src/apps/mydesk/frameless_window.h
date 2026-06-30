#pragma once

#include <QWidget>

class QHBoxLayout;
class QVBoxLayout;
class QLabel;
class QPushButton;

// 无边框主窗口基类：自定义标题栏 + 窗口拖动 + 圆角 + 阴影
class FramelessWindow : public QWidget {
    Q_OBJECT
public:
    explicit FramelessWindow(QWidget* parent = nullptr);
    ~FramelessWindow() override = default;

    // 设置标题文字
    void setWindowTitle(const QString& title);
    QString windowTitle() const;

    // 设置中央内容 widget
    void setCentralWidget(QWidget* widget);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onMinimize();
    void onMaximizeRestore();
    void onClose();

private:
    QWidget* buildTitleBar();
    void updateMaximizeButton();

    // Title bar widgets
    QWidget* titleBar_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* iconLabel_ = nullptr;
    QPushButton* minimizeBtn_ = nullptr;
    QPushButton* maximizeBtn_ = nullptr;
    QPushButton* closeBtn_ = nullptr;

    // Central content area
    QWidget* centralWidget_ = nullptr;
    QVBoxLayout* mainLayout_ = nullptr;

    // Drag state
    bool dragging_ = false;
    QPoint dragStartPos_;
    QPoint windowStartPos_;

    // Shadow margin (for rounded corners & shadow)
    static constexpr int kShadowMargin = 10;
    static constexpr int kBorderRadius = 10;
};
