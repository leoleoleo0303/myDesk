#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;
class QStackedWidget;

namespace rd {

class AccountManager;

// 登录对话框 - 支持登录/注册切换
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(AccountManager* accountMgr, QWidget* parent = nullptr);

    QString username() const;

private slots:
    void onLogin();
    void onRegister();
    void onSwitchToRegister();
    void onSwitchToLogin();
    void onLoginSuccess(const QString& username);
    void onLoginFailed(const QString& reason);
    void onRegisterSuccess();
    void onRegisterFailed(const QString& reason);
    void onSkipLogin();

private:
    QWidget* buildLoginPage();
    QWidget* buildRegisterPage();

    AccountManager* accountMgr_ = nullptr;
    QStackedWidget* stack_ = nullptr;

    // Login page
    QLineEdit* loginUsernameEdit_ = nullptr;
    QLineEdit* loginPasswordEdit_ = nullptr;
    QPushButton* loginBtn_ = nullptr;
    QLabel* loginStatus_ = nullptr;

    // Register page
    QLineEdit* regUsernameEdit_ = nullptr;
    QLineEdit* regPasswordEdit_ = nullptr;
    QLineEdit* regConfirmEdit_ = nullptr;
    QLineEdit* regEmailEdit_ = nullptr;
    QPushButton* registerBtn_ = nullptr;
    QLabel* regStatus_ = nullptr;
};

}  // namespace rd
