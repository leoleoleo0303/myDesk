#include "frameless_window.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

FramelessWindow::FramelessWindow(QWidget* parent) : QWidget(parent) {
    // 无边框 + 透明背景（用于绘制圆角和阴影）
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);

    // 主布局：标题栏 + 内容区
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(kShadowMargin, kShadowMargin,
                                    kShadowMargin, kShadowMargin);
    mainLayout_->setSpacing(0);

    // 创建一个内容容器用于绘制圆角背景
    auto* container = new QWidget;
    container->setObjectName("framelessContainer");
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    titleBar_ = buildTitleBar();
    containerLayout->addWidget(titleBar_);

    // 阴影效果
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(kShadowMargin * 2);
    shadow->setColor(QColor(0, 0, 0, 80));
    shadow->setOffset(0, 2);
    container->setGraphicsEffect(shadow);

    container->setStyleSheet(
        QString("#framelessContainer {"
                "  background-color: #f5f6fa;"
                "  border-radius: %1px;"
                "}")
            .arg(kBorderRadius));

    mainLayout_->addWidget(container);

    // 保存 container 的 layout 供 setCentralWidget 使用
    // 把 containerLayout 存为成员变量（通过 container 找回来）
    centralWidget_ = nullptr;
}

void FramelessWindow::setWindowTitle(const QString& title) {
    QWidget::setWindowTitle(title);
    if (titleLabel_) titleLabel_->setText(title);
}

QString FramelessWindow::windowTitle() const {
    return QWidget::windowTitle();
}

void FramelessWindow::setCentralWidget(QWidget* widget) {
    if (!widget) return;

    // 找到 container 的 layout
    auto* container = findChild<QWidget*>("framelessContainer");
    if (!container) return;
    auto* containerLayout = qobject_cast<QVBoxLayout*>(container->layout());
    if (!containerLayout) return;

    if (centralWidget_) {
        containerLayout->removeWidget(centralWidget_);
        centralWidget_->hide();
    }

    centralWidget_ = widget;
    centralWidget_->setParent(container);
    containerLayout->addWidget(centralWidget_, 1);
}

QWidget* FramelessWindow::buildTitleBar() {
    auto* bar = new QWidget;
    bar->setObjectName("titleBar");
    bar->setFixedHeight(40);
    bar->setStyleSheet(
        QString("#titleBar {"
                "  background-color: #2c3e50;"
                "  border-top-left-radius: %1px;"
                "  border-top-right-radius: %1px;"
                "}")
            .arg(kBorderRadius));

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 0, 4, 0);
    layout->setSpacing(8);

    // App icon
    iconLabel_ = new QLabel;
    iconLabel_->setText("🖥");
    iconLabel_->setStyleSheet("font-size: 16px;");
    layout->addWidget(iconLabel_);

    // Title text
    titleLabel_ = new QLabel("myDesk");
    titleLabel_->setStyleSheet(
        "color: white; font-size: 13px; font-weight: bold;");
    layout->addWidget(titleLabel_);

    layout->addStretch();

    // Button style helper
    auto btnStyle = [](const QString& hoverBg) {
        return QString(
            "QPushButton {"
            "  background: transparent;"
            "  border: none;"
            "  color: #bdc3c7;"
            "  font-size: 16px;"
            "  font-weight: bold;"
            "  padding: 4px 10px;"
            "  border-radius: 4px;"
            "}"
            "QPushButton:hover {"
            "  background-color: %1;"
            "  color: white;"
            "}")
            .arg(hoverBg);
    };

    // Minimize button
    minimizeBtn_ = new QPushButton("─");
    minimizeBtn_->setFixedSize(36, 28);
    minimizeBtn_->setToolTip("Minimize");
    minimizeBtn_->setStyleSheet(btnStyle("#3d566e"));
    connect(minimizeBtn_, &QPushButton::clicked, this,
            &FramelessWindow::onMinimize);
    layout->addWidget(minimizeBtn_);

    // Maximize button
    maximizeBtn_ = new QPushButton("□");
    maximizeBtn_->setFixedSize(36, 28);
    maximizeBtn_->setToolTip("Maximize");
    maximizeBtn_->setStyleSheet(btnStyle("#3d566e"));
    connect(maximizeBtn_, &QPushButton::clicked, this,
            &FramelessWindow::onMaximizeRestore);
    layout->addWidget(maximizeBtn_);

    // Close button
    closeBtn_ = new QPushButton("✕");
    closeBtn_->setFixedSize(36, 28);
    closeBtn_->setToolTip("Close");
    closeBtn_->setStyleSheet(btnStyle("#e74c3c"));
    connect(closeBtn_, &QPushButton::clicked, this, &FramelessWindow::onClose);
    layout->addWidget(closeBtn_);

    return bar;
}

void FramelessWindow::onMinimize() {
    showMinimized();
}

void FramelessWindow::onMaximizeRestore() {
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
    updateMaximizeButton();
}

void FramelessWindow::onClose() {
    close();
}

void FramelessWindow::updateMaximizeButton() {
    if (isMaximized()) {
        maximizeBtn_->setText("❐");
        maximizeBtn_->setToolTip("Restore");
    } else {
        maximizeBtn_->setText("□");
        maximizeBtn_->setToolTip("Maximize");
    }
}

// --- 窗口拖动 ---

void FramelessWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // 只有点击标题栏区域才能拖动
        QPoint localPos = event->pos();
        // 标题栏位于 shadow margin 内侧的顶部 40px 区域
        QRect titleRect(kShadowMargin, kShadowMargin,
                        width() - 2 * kShadowMargin, 40);
        if (titleRect.contains(localPos)) {
            dragging_ = true;
            dragStartPos_ = event->globalPos();
            windowStartPos_ = pos();
        }
    }
    QWidget::mousePressEvent(event);
}

void FramelessWindow::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_) {
        QPoint delta = event->globalPos() - dragStartPos_;
        move(windowStartPos_ + delta);
    }
    QWidget::mouseMoveEvent(event);
}

void FramelessWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void FramelessWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    // 双击标题栏最大化/还原
    QRect titleRect(kShadowMargin, kShadowMargin,
                    width() - 2 * kShadowMargin, 40);
    if (titleRect.contains(event->pos())) {
        onMaximizeRestore();
    }
    QWidget::mouseDoubleClickEvent(event);
}

// --- 圆角背景绘制 ---

void FramelessWindow::paintEvent(QPaintEvent* /*event*/) {
    // 绘制透明底层（阴影由 QGraphicsDropShadowEffect 处理）
    // 这里不需要额外绘制，container 的 stylesheet 已处理圆角背景
}
