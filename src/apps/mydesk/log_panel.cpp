#include "log_panel.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace rd {

static LogPanel* g_logPanel = nullptr;

LogPanel* globalLogPanel() { return g_logPanel; }
void setGlobalLogPanel(LogPanel* panel) { g_logPanel = panel; }

LogPanel::LogPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    // 工具栏
    auto* toolbar = new QHBoxLayout;

    filterBox_ = new QComboBox;
    filterBox_->addItem("All (Debug+)");
    filterBox_->addItem("Info+");
    filterBox_->addItem("Warning+");
    filterBox_->addItem("Error only");
    connect(filterBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogPanel::onFilterChanged);
    toolbar->addWidget(filterBox_);

    toolbar->addStretch();

    copySelectedBtn_ = new QPushButton("Copy Selected");
    copySelectedBtn_->setFixedHeight(28);
    copySelectedBtn_->setStyleSheet(
        "QPushButton { background-color: #2ecc71; color: white; "
        "border-radius: 3px; padding: 2px 8px; font-size: 12px; }"
        "QPushButton:hover { background-color: #27ae60; }");
    connect(copySelectedBtn_, &QPushButton::clicked, this, &LogPanel::onCopySelected);
    toolbar->addWidget(copySelectedBtn_);

    copyAllBtn_ = new QPushButton("📋 Copy All Logs");
    copyAllBtn_->setFixedHeight(28);
    copyAllBtn_->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; "
        "border-radius: 3px; padding: 2px 8px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2980b9; }");
    connect(copyAllBtn_, &QPushButton::clicked, this, &LogPanel::onCopyAll);
    toolbar->addWidget(copyAllBtn_);

    clearBtn_ = new QPushButton("Clear");
    clearBtn_->setFixedHeight(28);
    clearBtn_->setStyleSheet(
        "QPushButton { background-color: #e74c3c; color: white; "
        "border-radius: 3px; padding: 2px 8px; font-size: 12px; }"
        "QPushButton:hover { background-color: #c0392b; }");
    connect(clearBtn_, &QPushButton::clicked, this, &LogPanel::onClear);
    toolbar->addWidget(clearBtn_);

    layout->addLayout(toolbar);

    // 日志显示区域
    logView_ = new QTextEdit;
    logView_->setReadOnly(true);
    logView_->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; "
        "font-family: 'Consolas', 'Courier New', monospace; font-size: 12px; "
        "border: 1px solid #333; border-radius: 4px; padding: 5px; }");
    logView_->setMinimumHeight(120);
    layout->addWidget(logView_);
}

void LogPanel::log(LogLevel level, const QString& message) {
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = level;
    entry.message = message;
    entries_.append(entry);

    // 根据过滤级别决定是否显示
    if (static_cast<int>(level) < static_cast<int>(filterLevel_)) return;

    QString timeStr = entry.timestamp.toString("hh:mm:ss.zzz");
    QString levelStr = levelToString(level);
    QString color = levelToColor(level);

    QString html = QString("<span style='color: #888;'>[%1]</span> "
                           "<span style='color: %2; font-weight: bold;'>[%3]</span> "
                           "<span style='color: %4;'>%5</span>")
                       .arg(timeStr, color, levelStr, color, message.toHtmlEscaped());

    logView_->append(html);

    // 自动滚动到底部
    auto* scrollBar = logView_->verticalScrollBar();
    if (scrollBar) scrollBar->setValue(scrollBar->maximum());
}

void LogPanel::clear() {
    entries_.clear();
    logView_->clear();
}

QString LogPanel::getAllLogs() const {
    QStringList lines;
    for (const auto& entry : entries_) {
        lines.append(QString("[%1] [%2] %3")
                         .arg(entry.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"),
                              levelToString(entry.level),
                              entry.message));
    }
    return lines.join("\n");
}

void LogPanel::onCopyAll() {
    QString text = getAllLogs();
    QApplication::clipboard()->setText(text);

    // 视觉反馈
    copyAllBtn_->setText("✓ Copied!");
    QTimer::singleShot(2000, this, [this]() {
        copyAllBtn_->setText("📋 Copy All Logs");
    });
}

void LogPanel::onCopySelected() {
    QString selected = logView_->textCursor().selectedText();
    if (!selected.isEmpty()) {
        // QTextEdit 的 selectedText 使用 Unicode 段落分隔符，替换为换行
        selected.replace(QChar(0x2029), '\n');
        QApplication::clipboard()->setText(selected);
    }
}

void LogPanel::onClear() {
    clear();
}

void LogPanel::onFilterChanged(int index) {
    filterLevel_ = static_cast<LogLevel>(index);

    // 重新渲染日志
    logView_->clear();
    for (const auto& entry : entries_) {
        if (static_cast<int>(entry.level) < static_cast<int>(filterLevel_)) continue;

        QString timeStr = entry.timestamp.toString("hh:mm:ss.zzz");
        QString levelStr = levelToString(entry.level);
        QString color = levelToColor(entry.level);

        QString html = QString("<span style='color: #888;'>[%1]</span> "
                               "<span style='color: %2; font-weight: bold;'>[%3]</span> "
                               "<span style='color: %4;'>%5</span>")
                           .arg(timeStr, color, levelStr, color, entry.message.toHtmlEscaped());
        logView_->append(html);
    }
}

QString LogPanel::levelToString(LogLevel level) const {
    switch (level) {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Info:    return "INFO";
    case LogLevel::Warning: return "WARN";
    case LogLevel::Error:   return "ERROR";
    }
    return "UNKNOWN";
}

QString LogPanel::levelToColor(LogLevel level) const {
    switch (level) {
    case LogLevel::Debug:   return "#888888";
    case LogLevel::Info:    return "#4fc3f7";
    case LogLevel::Warning: return "#ffa726";
    case LogLevel::Error:   return "#ef5350";
    }
    return "#d4d4d4";
}

}  // namespace rd
