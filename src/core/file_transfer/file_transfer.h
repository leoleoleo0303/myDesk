#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "core/net/channel.h"

namespace rd {

// 单个文件传输任务的状态
struct FileTransferInfo {
    uint32_t id = 0;
    std::string fileName;
    uint64_t fileSize = 0;
    uint64_t transferred = 0;
    std::string savePath;  // 接收方保存路径
    bool isSender = false;
    enum State { Pending, Transferring, Completed, Failed, Cancelled } state = Pending;
};

// 文件传输管理器 - 支持双向传输
// 发送文件: offerFile() -> 等待对端 accept -> 分块发送
// 接收文件: 收到 FileOffer -> 用户选择路径 accept -> 接收数据块
class FileTransferManager : public QObject {
    Q_OBJECT
public:
    explicit FileTransferManager(QObject* parent = nullptr);
    ~FileTransferManager() override;

    // 设置传输通道（复用已有的 IChannel）
    void setChannel(IChannel* ch) { channel_ = ch; }

    // 发送文件（发送方调用）
    // 返回传输 ID
    uint32_t offerFile(const std::string& filePath);

    // 接受传输（接收方调用）
    void acceptFile(uint32_t transferId, const std::string& savePath);

    // 拒绝传输（接收方调用）
    void rejectFile(uint32_t transferId);

    // 取消传输（任意一方）
    void cancelTransfer(uint32_t transferId);

    // 处理收到的文件传输相关消息
    void handleMessage(net::MsgType type, const std::vector<uint8_t>& payload);

    // 获取所有传输任务
    std::vector<FileTransferInfo> getTransfers() const;

signals:
    // 收到对端的文件发送请求
    void fileOffered(uint32_t id, const QString& fileName, uint64_t fileSize);
    // 传输进度更新
    void transferProgress(uint32_t id, uint64_t transferred, uint64_t total);
    // 传输完成
    void transferCompleted(uint32_t id);
    // 传输失败/取消
    void transferFailed(uint32_t id, const QString& reason);

private:
    void sendFileData(uint32_t transferId);

    IChannel* channel_ = nullptr;
    mutable std::mutex mtx_;
    std::unordered_map<uint32_t, FileTransferInfo> transfers_;
    std::unordered_map<uint32_t, std::thread> sendThreads_;
    uint32_t nextId_ = 1;

    static constexpr uint32_t kChunkSize = 64 * 1024;  // 64KB per chunk
};

}  // namespace rd
