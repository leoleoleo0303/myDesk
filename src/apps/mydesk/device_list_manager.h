#pragma once

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>

#include <string>
#include <vector>

namespace rd {

// 保存的设备信息
struct SavedDevice {
    std::string deviceId;       // 设备ID或IP
    std::string alias;          // 别名（用户自定义）
    std::string lastAddress;    // 最后连接的地址
    uint16_t lastPort = 9000;   // 最后连接的端口
    std::string lastPassword;   // 最后使用的密码（可选保存）
    int connectionMode = 0;     // 0=LAN, 1=P2P
    int64_t lastConnected = 0;  // 上次连接时间戳
    bool savePassword = false;  // 是否保存密码
};

// 设备列表管理器 - 保存常用设备，支持一键连接
class DeviceListManager : public QObject {
    Q_OBJECT
public:
    explicit DeviceListManager(QObject* parent = nullptr);
    ~DeviceListManager() override;

    // 加载/保存设备列表（本地 JSON 文件）
    void load();
    void save();

    // 添加/更新设备
    void addDevice(const SavedDevice& device);
    void updateDevice(const std::string& deviceId, const SavedDevice& device);
    void removeDevice(const std::string& deviceId);

    // 获取设备列表
    const std::vector<SavedDevice>& devices() const { return devices_; }
    const SavedDevice* findDevice(const std::string& deviceId) const;

    // 更新最后连接时间
    void markConnected(const std::string& deviceId);

signals:
    void deviceListChanged();

private:
    std::string getConfigPath() const;

    std::vector<SavedDevice> devices_;
};

}  // namespace rd
