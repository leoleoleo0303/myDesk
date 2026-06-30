#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QDateTime>

class QTextEdit;
class QPushButton;
class QVBoxLayout;
class QComboBox;

namespace rd {

// 日志级别
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

// 开发者日志面板 - 在界面上显示运行日志，支持一键复制
// 仅在开发者模式下显示
class LogPanel : public QWidget {
    Q_OBJECT
public:
    explicit LogPanel(QWidget* parent = nullptr);

    // 添加日志
    void log(LogLevel level, const QString& message);
    void debug(const QString& msg) { log(LogLevel::Debug, msg); }
    void info(const QString& msg) { log(LogLevel::Info, msg); }
    void warning(const QString& msg) { log(LogLevel::Warning, msg); }
    void error(const QString& msg) { log(LogLevel::Error, msg); }

    // 清空日志
    void clear();

    // 获取所有日志文本
    QString getAllLogs() const;

    // 设置日志级别过滤
    void setFilterLevel(LogLevel level) { filterLevel_ = level; }

public slots:
    void onCopyAll();
    void onCopySelected();
    void onClear();
    void onFilterChanged(int index);

private:
    QString levelToString(LogLevel level) const;
    QString levelToColor(LogLevel level) const;

    QTextEdit* logView_ = nullptr;
    QPushButton* copyAllBtn_ = nullptr;
    QPushButton* copySelectedBtn_ = nullptr;
    QPushButton* clearBtn_ = nullptr;
    QComboBox* filterBox_ = nullptr;
    LogLevel filterLevel_ = LogLevel::Debug;

    struct LogEntry {
        QDateTime timestamp;
        LogLevel level;
        QString message;
    };
    QList<LogEntry> entries_;
};

// 全局日志实例（单例）
LogPanel* globalLogPanel();
void setGlobalLogPanel(LogPanel* panel);

// 便捷宏
#define LOG_DEBUG(msg) do { if (rd::globalLogPanel()) rd::globalLogPanel()->debug(msg); } while(0)
#define LOG_INFO(msg)  do { if (rd::globalLogPanel()) rd::globalLogPanel()->info(msg); } while(0)
#define LOG_WARN(msg)  do { if (rd::globalLogPanel()) rd::globalLogPanel()->warning(msg); } while(0)
#define LOG_ERROR(msg) do { if (rd::globalLogPanel()) rd::globalLogPanel()->error(msg); } while(0)

}  // namespace rd
