#include "device_list_manager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

#include <chrono>

namespace rd {

DeviceListManager::DeviceListManager(QObject* parent) : QObject(parent) {
    load();
}

DeviceListManager::~DeviceListManager() {
    save();
}

void DeviceListManager::load() {
    std::string path = getConfigPath();
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) return;

    devices_.clear();
    QJsonArray arr = doc.array();
    for (const auto& val : arr) {
        QJsonObject obj = val.toObject();
        SavedDevice dev;
        dev.deviceId = obj["deviceId"].toString().toStdString();
        dev.alias = obj["alias"].toString().toStdString();
        dev.lastAddress = obj["lastAddress"].toString().toStdString();
        dev.lastPort = static_cast<uint16_t>(obj["lastPort"].toInt(9000));
        dev.connectionMode = obj["connectionMode"].toInt(0);
        dev.lastConnected = static_cast<int64_t>(obj["lastConnected"].toDouble(0));
        dev.savePassword = obj["savePassword"].toBool(false);
        if (dev.savePassword) {
            dev.lastPassword = obj["lastPassword"].toString().toStdString();
        }
        devices_.push_back(dev);
    }
}

void DeviceListManager::save() {
    std::string path = getConfigPath();

    // 确保目录存在
    QString qpath = QString::fromStdString(path);
    QDir dir = QFileInfo(qpath).dir();
    if (!dir.exists()) dir.mkpath(".");

    QJsonArray arr;
    for (const auto& dev : devices_) {
        QJsonObject obj;
        obj["deviceId"] = QString::fromStdString(dev.deviceId);
        obj["alias"] = QString::fromStdString(dev.alias);
        obj["lastAddress"] = QString::fromStdString(dev.lastAddress);
        obj["lastPort"] = dev.lastPort;
        obj["connectionMode"] = dev.connectionMode;
        obj["lastConnected"] = static_cast<double>(dev.lastConnected);
        obj["savePassword"] = dev.savePassword;
        if (dev.savePassword) {
            obj["lastPassword"] = QString::fromStdString(dev.lastPassword);
        }
        arr.append(obj);
    }

    QFile file(qpath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
        file.close();
    }
}

void DeviceListManager::addDevice(const SavedDevice& device) {
    // 如果已存在则更新
    for (auto& d : devices_) {
        if (d.deviceId == device.deviceId) {
            d = device;
            save();
            emit deviceListChanged();
            return;
        }
    }
    devices_.push_back(device);
    save();
    emit deviceListChanged();
}

void DeviceListManager::updateDevice(const std::string& deviceId, const SavedDevice& device) {
    for (auto& d : devices_) {
        if (d.deviceId == deviceId) {
            d = device;
            save();
            emit deviceListChanged();
            return;
        }
    }
}

void DeviceListManager::removeDevice(const std::string& deviceId) {
    devices_.erase(
        std::remove_if(devices_.begin(), devices_.end(),
                       [&](const SavedDevice& d) { return d.deviceId == deviceId; }),
        devices_.end());
    save();
    emit deviceListChanged();
}

const SavedDevice* DeviceListManager::findDevice(const std::string& deviceId) const {
    for (const auto& d : devices_) {
        if (d.deviceId == deviceId) return &d;
    }
    return nullptr;
}

void DeviceListManager::markConnected(const std::string& deviceId) {
    for (auto& d : devices_) {
        if (d.deviceId == deviceId) {
            d.lastConnected = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            save();
            emit deviceListChanged();
            return;
        }
    }
}

std::string DeviceListManager::getConfigPath() const {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return (configDir + "/mydesk/devices.json").toStdString();
}

}  // namespace rd
