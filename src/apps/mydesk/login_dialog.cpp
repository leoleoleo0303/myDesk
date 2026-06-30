#include "login_dialog.h"
#include "account_manager.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace rd {

LoginDialog::LoginDialog(AccountManager* accountMgr, QWidget* parent)
    : QDialog(parent), accountMgr_(accountMgr) {
    setWindowTitle("myDesk - Login");
    setFixedSize(420, 380);
    setStyleSheet(
        "QDialog { background-color: #f8f9fa; }"
        "QLineEdit { border: 1px solid #dfe6e9; border-radius: 6px; "
        "padding: 10px; font-size: 14px; background: white; }"
        "QLineEdit:focus { border-color: #3498db; }"
        "QLabel { font-size: 13px; color: #2c3e50; }");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(30, 25, 30, 25);

    // Title
    auto* title = new QLabel("myDesk");
    title->setStyleSheet("font-size: 28px; font-weight: bold; color: #2c3e50;");
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    auto* subtitle = new QLabel("Cross-Platform Remote Desktop");
    subtitle->setStyleSheet("font-size: 12px; color: #7f8c8d;");
    subtitle->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(subtitle);

    mainLayout->addSpacing(10);

    stack_ = new QStackedWidget;
    stack_->addWidget(buildLoginPage());
    stack_->addWidget(buildRegisterPage());
    mainLayout->addWidget(stack_);

    // Skip login button (for development)
    auto* skipBtn = new QPushButton("Skip Login (Dev Mode)");
    skipBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #7f8c8d; "
        "border: none; font-size: 12px; text-decoration: underline; }"
        "QPushButton:hover { color: #3498db; }");
    connect(skipBtn, &QPushButton::clicked, this, &LoginDialog::onSkipLogin);
    mainLayout->addWidget(skipBtn, 0, Qt::AlignCenter);

    // Connect account manager signals
    connect(accountMgr_, &AccountManager::loginSuccess,
            this, &LoginDialog::onLoginSuccess);
    connect(accountMgr_, &AccountManager::loginFailed,
            this, &LoginDialog::onLoginFailed);
    connect(accountMgr_, &AccountManager::registerSuccess,
            this, &LoginDialog::onRegisterSuccess);
    connect(accountMgr_, &AccountManager::registerFailed,
            this, &LoginDialog::onRegisterFailed);
}

QString LoginDialog::username() const {
    return QString::fromStdString(accountMgr_->accountInfo().username);
}

QWidget* LoginDialog::buildLoginPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    loginUsernameEdit_ = new QLineEdit;
    loginUsernameEdit_->setPlaceholderText("Username");
    layout->addWidget(loginUsernameEdit_);

    loginPasswordEdit_ = new QLineEdit;
    loginPasswordEdit_->setPlaceholderText("Password");
    loginPasswordEdit_->setEchoMode(QLineEdit::Password);
    layout->addWidget(loginPasswordEdit_);

    loginBtn_ = new QPushButton("Login");
    loginBtn_->setMinimumHeight(42);
    loginBtn_->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; "
        "border-radius: 6px; font-size: 15px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:disabled { background-color: #bdc3c7; }");
    connect(loginBtn_, &QPushButton::clicked, this, &LoginDialog::onLogin);
    layout->addWidget(loginBtn_);

    loginStatus_ = new QLabel("");
    loginStatus_->setStyleSheet("color: #e74c3c; font-size: 12px;");
    loginStatus_->setAlignment(Qt::AlignCenter);
    layout->addWidget(loginStatus_);

    auto* switchBtn = new QPushButton("Don't have an account? Register");
    switchBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #3498db; "
        "border: none; font-size: 12px; }"
        "QPushButton:hover { text-decoration: underline; }");
    connect(switchBtn, &QPushButton::clicked, this, &LoginDialog::onSwitchToRegister);
    layout->addWidget(switchBtn, 0, Qt::AlignCenter);

    return page;
}

QWidget* LoginDialog::buildRegisterPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(10);

    regUsernameEdit_ = new QLineEdit;
    regUsernameEdit_->setPlaceholderText("Username");
    layout->addWidget(regUsernameEdit_);

    regEmailEdit_ = new QLineEdit;
    regEmailEdit_->setPlaceholderText("Email (optional)");
    layout->addWidget(regEmailEdit_);

    regPasswordEdit_ = new QLineEdit;
    regPasswordEdit_->setPlaceholderText("Password (6+ characters)");
    regPasswordEdit_->setEchoMode(QLineEdit::Password);
    layout->addWidget(regPasswordEdit_);

    regConfirmEdit_ = new QLineEdit;
    regConfirmEdit_->setPlaceholderText("Confirm Password");
    regConfirmEdit_->setEchoMode(QLineEdit::Password);
    layout->addWidget(regConfirmEdit_);

    registerBtn_ = new QPushButton("Register");
    registerBtn_->setMinimumHeight(42);
    registerBtn_->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; "
        "border-radius: 6px; font-size: 15px; font-weight: bold; }"
        "QPushButton:hover { background-color: #229954; }"
        "QPushButton:disabled { background-color: #bdc3c7; }");
    connect(registerBtn_, &QPushButton::clicked, this, &LoginDialog::onRegister);
    layout->addWidget(registerBtn_);

    regStatus_ = new QLabel("");
    regStatus_->setStyleSheet("color: #e74c3c; font-size: 12px;");
    regStatus_->setAlignment(Qt::AlignCenter);
    layout->addWidget(regStatus_);

    auto* switchBtn = new QPushButton("Already have an account? Login");
    switchBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #3498db; "
        "border: none; font-size: 12px; }"
        "QPushButton:hover { text-decoration: underline; }");
    connect(switchBtn, &QPushButton::clicked, this, &LoginDialog::onSwitchToLogin);
    layout->addWidget(switchBtn, 0, Qt::AlignCenter);

    return page;
}

void LoginDialog::onLogin() {
    std::string user = loginUsernameEdit_->text().toStdString();
    std::string pass = loginPasswordEdit_->text().toStdString();

    if (user.empty() || pass.empty()) {
        loginStatus_->setText("Please enter username and password");
        return;
    }

    loginBtn_->setEnabled(false);
    loginStatus_->setText("Logging in...");
    loginStatus_->setStyleSheet("color: #f39c12; font-size: 12px;");
    accountMgr_->login(user, pass);
}

void LoginDialog::onRegister() {
    std::string user = regUsernameEdit_->text().toStdString();
    std::string pass = regPasswordEdit_->text().toStdString();
    std::string confirm = regConfirmEdit_->text().toStdString();
    std::string email = regEmailEdit_->text().toStdString();

    if (user.empty() || pass.empty()) {
        regStatus_->setText("Please fill in required fields");
        return;
    }
    if (pass != confirm) {
        regStatus_->setText("Passwords do not match");
        return;
    }

    registerBtn_->setEnabled(false);
    regStatus_->setText("Registering...");
    regStatus_->setStyleSheet("color: #f39c12; font-size: 12px;");
    accountMgr_->registerAccount(user, pass, email);
}

void LoginDialog::onSwitchToRegister() {
    stack_->setCurrentIndex(1);
}

void LoginDialog::onSwitchToLogin() {
    stack_->setCurrentIndex(0);
}

void LoginDialog::onLoginSuccess(const QString& /*username*/) {
    accept();  // Close dialog with success
}

void LoginDialog::onLoginFailed(const QString& reason) {
    loginStatus_->setText(reason);
    loginStatus_->setStyleSheet("color: #e74c3c; font-size: 12px;");
    loginBtn_->setEnabled(true);
}

void LoginDialog::onRegisterSuccess() {
    regStatus_->setText("Registration successful! Please login.");
    regStatus_->setStyleSheet("color: #27ae60; font-size: 12px;");
    registerBtn_->setEnabled(true);

    // 自动切换到登录页
    QTimer::singleShot(1500, this, [this]() {
        stack_->setCurrentIndex(0);
        loginUsernameEdit_->setText(regUsernameEdit_->text());
    });
}

void LoginDialog::onRegisterFailed(const QString& reason) {
    regStatus_->setText(reason);
    regStatus_->setStyleSheet("color: #e74c3c; font-size: 12px;");
    registerBtn_->setEnabled(true);
}

void LoginDialog::onSkipLogin() {
    // 跳过登录，直接使用开发者模式
    accountMgr_->login("developer", "dev");
}

}  // namespace rd
