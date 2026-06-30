#include "account_manager.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <thread>

namespace rd {

AccountManager::AccountManager(QObject* parent) : QObject(parent) {}

AccountManager::~AccountManager() = default;

void AccountManager::login(const std::string& username, const std::string& password) {
    // TODO: 后期对接服务器
    // 当前本地逻辑：任意用户名密码都可以登录（开发测试用）
    // 后期实现:
    //   QNetworkAccessManager 发送 POST 请求到 serverUrl_ + "/api/login"
    //   body: { "username": username, "password": password }
    //   成功返回: { "token": "xxx", "mode": "developer" | "user" }
    //   失败返回: { "error": "Invalid credentials" }

    // 模拟异步登录
    std::thread([this, username, password]() {
        // 模拟网络延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 本地验证逻辑（后期替换为服务器验证）
        if (username.empty() || password.empty()) {
            emit loginFailed("Username and password cannot be empty");
            return;
        }

        // 简单本地验证（开发阶段，任意非空账密都通过）
        account_.username = username;
        account_.token = "local_dev_token_" + username;
        account_.isLoggedIn = true;
        account_.mode = UserMode::Developer;  // 默认开发者模式

        emit loginSuccess(QString::fromStdString(username));
    }).detach();
}

void AccountManager::logout() {
    // TODO: 后期通知服务器注销 token
    // POST serverUrl_ + "/api/logout" { "token": account_.token }

    account_.username.clear();
    account_.token.clear();
    account_.isLoggedIn = false;
    emit logoutSuccess();
}

void AccountManager::registerAccount(const std::string& username,
                                     const std::string& password,
                                     const std::string& email) {
    // TODO: 后期对接服务器
    // POST serverUrl_ + "/api/register"
    // body: { "username": username, "password": password, "email": email }

    std::thread([this, username, password, email]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (username.empty() || password.empty()) {
            emit registerFailed("Username and password cannot be empty");
            return;
        }
        if (password.length() < 6) {
            emit registerFailed("Password must be at least 6 characters");
            return;
        }

        // 本地模拟注册成功
        emit registerSuccess();
    }).detach();
}

void AccountManager::setUserMode(UserMode mode) {
    account_.mode = mode;
    emit modeChanged(static_cast<int>(mode));
}

}  // namespace rd
