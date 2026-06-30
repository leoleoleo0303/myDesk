#pragma once

#include <QObject>
#include <QString>

#include <string>
#include <functional>

namespace rd {

// 用户模式
enum class UserMode {
    Developer,  // 开发者模式：显示详细信息、日志面板
    User        // 用户模式：简洁操作界面
};

// 账号信息
struct AccountInfo {
    std::string username;
    std::string token;        // 登录后的 session token
    UserMode mode = UserMode::Developer;  // 默认开发者模式
    bool isLoggedIn = false;
};

// 账号管理器 - 处理登录/登出逻辑
// 当前为本地逻辑框架，后期对接服务器 API
class AccountManager : public QObject {
    Q_OBJECT
public:
    explicit AccountManager(QObject* parent = nullptr);
    ~AccountManager() override;

    // 登录（异步，结果通过信号通知）
    // 后期: POST /api/login { username, password } -> { token, mode }
    void login(const std::string& username, const std::string& password);

    // 登出
    void logout();

    // 注册（异步）
    // 后期: POST /api/register { username, password, email }
    void registerAccount(const std::string& username, const std::string& password,
                         const std::string& email = "");

    // 获取当前账号信息
    const AccountInfo& accountInfo() const { return account_; }
    bool isLoggedIn() const { return account_.isLoggedIn; }
    UserMode userMode() const { return account_.mode; }

    // 切换模式（开发者/用户）
    void setUserMode(UserMode mode);

    // 设置服务器地址（后期使用）
    void setServerUrl(const std::string& url) { serverUrl_ = url; }

signals:
    void loginSuccess(const QString& username);
    void loginFailed(const QString& reason);
    void logoutSuccess();
    void registerSuccess();
    void registerFailed(const QString& reason);
    void modeChanged(int mode);  // 0=Developer, 1=User

private:
    AccountInfo account_;
    std::string serverUrl_ = "http://localhost:8080";  // 后期服务器地址
};

}  // namespace rd
