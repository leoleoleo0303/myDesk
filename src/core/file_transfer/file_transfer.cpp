#include "file_transfer.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <cstring>
#include <fstream>

namespace rd {

FileTransferManager::FileTransferManager(QObject* parent)
    : QObject(parent) {}

FileTransferManager::~FileTransferManager() {
    // 等待所有发送线程结束
    for (auto& [id, th] : sendThreads_) {
        if (th.joinable()) th.join();
    }
}

uint32_t FileTransferManager::offerFile(const std::string& filePath) {
    // 获取文件名和大小
    std::ifstream f(filePath, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return 0;

    uint64_t fileSize = static_cast<uint64_t>(f.tellg());
    f.close();

    // 从路径提取文件名
    std::string fileName = filePath;
    auto pos = fileName.find_last_of("/\\");
    if (pos != std::string::npos) fileName = fileName.substr(pos + 1);

    uint32_t id;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        id = nextId_++;
        FileTransferInfo info;
        info.id = id;
        info.fileName = fileName;
        info.fileSize = fileSize;
        info.isSender = true;
        info.savePath = filePath;  // 发送方存本地路径
        info.state = FileTransferInfo::Pending;
        transfers_[id] = info;
    }

    // 发送 FileOffer 消息 (不包含本地路径，只发文件名和大小)
    QJsonObject obj;
    obj["id"] = static_cast<int>(id);
    obj["name"] = QString::fromStdString(fileName);
    obj["size"] = static_cast<qint64>(fileSize);

    QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    if (channel_) {
        channel_->sendMessage(net::MsgType::FileOffer,
                              json.constData(), static_cast<uint32_t>(json.size()));
    }

    return id;
}

void FileTransferManager::acceptFile(uint32_t transferId, const std::string& savePath) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = transfers_.find(transferId);
        if (it == transfers_.end()) return;
        it->second.savePath = savePath;
        it->second.state = FileTransferInfo::Transferring;
    }

    QJsonObject obj;
    obj["id"] = static_cast<int>(transferId);
    obj["savePath"] = QString::fromStdString(savePath);

    QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    if (channel_) {
        channel_->sendMessage(net::MsgType::FileAccept,
                              json.constData(), static_cast<uint32_t>(json.size()));
    }
}

void FileTransferManager::rejectFile(uint32_t transferId) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = transfers_.find(transferId);
        if (it == transfers_.end()) return;
        it->second.state = FileTransferInfo::Cancelled;
    }

    QJsonObject obj;
    obj["id"] = static_cast<int>(transferId);

    QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    if (channel_) {
        channel_->sendMessage(net::MsgType::FileReject,
                              json.constData(), static_cast<uint32_t>(json.size()));
    }

    emit transferFailed(transferId, "Rejected by remote");
}

void FileTransferManager::cancelTransfer(uint32_t transferId) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = transfers_.find(transferId);
        if (it == transfers_.end()) return;
        it->second.state = FileTransferInfo::Cancelled;
    }

    QJsonObject obj;
    obj["id"] = static_cast<int>(transferId);
    obj["reason"] = "User cancelled";

    QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    if (channel_) {
        channel_->sendMessage(net::MsgType::FileCancel,
                              json.constData(), static_cast<uint32_t>(json.size()));
    }

    emit transferFailed(transferId, "Cancelled");
}

void FileTransferManager::handleMessage(net::MsgType type, const std::vector<uint8_t>& payload) {
    switch (type) {
    case net::MsgType::FileOffer: {
        QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray(reinterpret_cast<const char*>(payload.data()),
                       static_cast<int>(payload.size())));
        QJsonObject obj = doc.object();
        uint32_t id = static_cast<uint32_t>(obj["id"].toInt());
        std::string name = obj["name"].toString().toStdString();
        uint64_t size = static_cast<uint64_t>(obj["size"].toDouble());

        {
            std::lock_guard<std::mutex> lk(mtx_);
            FileTransferInfo info;
            info.id = id;
            info.fileName = name;
            info.fileSize = size;
            info.isSender = false;
            info.state = FileTransferInfo::Pending;
            transfers_[id] = info;
        }

        emit fileOffered(id, QString::fromStdString(name), size);
        break;
    }
    case net::MsgType::FileAccept: {
        QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray(reinterpret_cast<const char*>(payload.data()),
                       static_cast<int>(payload.size())));
        QJsonObject obj = doc.object();
        uint32_t id = static_cast<uint32_t>(obj["id"].toInt());

        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = transfers_.find(id);
            if (it == transfers_.end()) break;
            it->second.state = FileTransferInfo::Transferring;
        }

        // 开始发送文件数据
        sendFileData(id);
        break;
    }
    case net::MsgType::FileReject: {
        QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray(reinterpret_cast<const char*>(payload.data()),
                       static_cast<int>(payload.size())));
        QJsonObject obj = doc.object();
        uint32_t id = static_cast<uint32_t>(obj["id"].toInt());

        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = transfers_.find(id);
            if (it != transfers_.end())
                it->second.state = FileTransferInfo::Cancelled;
        }
        emit transferFailed(id, "Rejected by remote");
        break;
    }
    case net::MsgType::FileData: {
        if (payload.size() < 12) break;  // id(4) + offset(8) + data

        uint32_t id;
        uint64_t offset;
        std::memcpy(&id, payload.data(), 4);
        std::memcpy(&offset, payload.data() + 4, 8);

        const uint8_t* data = payload.data() + 12;
        uint32_t dataLen = static_cast<uint32_t>(payload.size() - 12);

        std::string savePath;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = transfers_.find(id);
            if (it == transfers_.end()) break;
            if (it->second.state == FileTransferInfo::Cancelled) break;
            savePath = it->second.savePath;
            it->second.transferred = offset + dataLen;
        }

        // 写入文件
        if (!savePath.empty()) {
            std::ofstream f(savePath, std::ios::binary | std::ios::in | std::ios::out);
            if (!f.is_open()) {
                // 首次写入，创建文件
                f.open(savePath, std::ios::binary | std::ios::out);
            }
            if (f.is_open()) {
                f.seekp(static_cast<std::streamoff>(offset));
                f.write(reinterpret_cast<const char*>(data), dataLen);
            }
        }

        uint64_t total;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            total = transfers_[id].fileSize;
        }
        emit transferProgress(id, offset + dataLen, total);
        break;
    }
    case net::MsgType::FileComplete: {
        QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray(reinterpret_cast<const char*>(payload.data()),
                       static_cast<int>(payload.size())));
        QJsonObject obj = doc.object();
        uint32_t id = static_cast<uint32_t>(obj["id"].toInt());

        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = transfers_.find(id);
            if (it != transfers_.end())
                it->second.state = FileTransferInfo::Completed;
        }
        emit transferCompleted(id);
        break;
    }
    case net::MsgType::FileCancel: {
        QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray(reinterpret_cast<const char*>(payload.data()),
                       static_cast<int>(payload.size())));
        QJsonObject obj = doc.object();
        uint32_t id = static_cast<uint32_t>(obj["id"].toInt());
        QString reason = obj["reason"].toString("Remote cancelled");

        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = transfers_.find(id);
            if (it != transfers_.end())
                it->second.state = FileTransferInfo::Cancelled;
        }
        emit transferFailed(id, reason);
        break;
    }
    default:
        break;
    }
}

void FileTransferManager::sendFileData(uint32_t transferId) {
    // 获取本地文件路径
    std::string filePath;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = transfers_.find(transferId);
        if (it == transfers_.end()) return;
        filePath = it->second.savePath;  // 发送方的 savePath 存的是本地文件路径
        it->second.state = FileTransferInfo::Transferring;
    }

    if (filePath.empty()) return;

    // 在后台线程发送文件数据
    sendThreads_[transferId] = std::thread([this, transferId, filePath]() {
        std::ifstream f(filePath, std::ios::binary);
        if (!f.is_open()) {
            emit transferFailed(transferId, "Cannot open file");
            return;
        }

        std::vector<uint8_t> buf(12 + kChunkSize);
        uint64_t offset = 0;
        uint64_t totalSize = 0;

        {
            std::lock_guard<std::mutex> lk(mtx_);
            totalSize = transfers_[transferId].fileSize;
        }

        std::memcpy(buf.data(), &transferId, 4);

        while (f.good() && !f.eof()) {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                auto it2 = transfers_.find(transferId);
                if (it2 == transfers_.end() || it2->second.state == FileTransferInfo::Cancelled)
                    return;
            }

            f.read(reinterpret_cast<char*>(buf.data() + 12), kChunkSize);
            auto bytesRead = static_cast<uint32_t>(f.gcount());
            if (bytesRead == 0) break;

            std::memcpy(buf.data() + 4, &offset, 8);

            if (channel_) {
                channel_->sendMessage(net::MsgType::FileData,
                                      buf.data(), 12 + bytesRead);
            }

            offset += bytesRead;
            {
                std::lock_guard<std::mutex> lk(mtx_);
                transfers_[transferId].transferred = offset;
            }
            emit transferProgress(transferId, offset, totalSize);

            // 简单流控：每发 1MB 暂停一下
            if (offset % (1024 * 1024) == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        // 发送完成标记
        QJsonObject obj;
        obj["id"] = static_cast<int>(transferId);
        QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        if (channel_) {
            channel_->sendMessage(net::MsgType::FileComplete,
                                  json.constData(), static_cast<uint32_t>(json.size()));
        }

        {
            std::lock_guard<std::mutex> lk(mtx_);
            transfers_[transferId].state = FileTransferInfo::Completed;
        }
        emit transferCompleted(transferId);
    });
}

std::vector<FileTransferInfo> FileTransferManager::getTransfers() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<FileTransferInfo> result;
    result.reserve(transfers_.size());
    for (const auto& [id, info] : transfers_) {
        result.push_back(info);
    }
    return result;
}

}  // namespace rd
