#include "device_id_gen.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>

#if defined(_WIN32)
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace rd {

namespace {

std::string getConfigDir() {
#if defined(_WIN32)
    char path[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path) == S_OK) {
        return std::string(path) + "\\myDesk";
    }
    return ".";
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : ".";
    }
    return std::string(home) + "/.config/mydesk";
#endif
}

void ensureDir(const std::string& dir) {
#if defined(_WIN32)
    CreateDirectoryA(dir.c_str(), nullptr);
#else
    ::mkdir(dir.c_str(), 0755);
#endif
}

std::mt19937& rng() {
    static std::mt19937 gen(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    return gen;
}

}  // namespace

std::string generateDeviceId() {
    std::uniform_int_distribution<int> dist(100000000, 999999999);
    return std::to_string(dist(rng()));
}

std::string generatePassword() {
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    std::uniform_int_distribution<int> dist(0, sizeof(chars) - 2);
    std::string pwd(6, ' ');
    for (auto& c : pwd) c = chars[dist(rng())];
    return pwd;
}

void DeviceIdentity::regeneratePassword() {
    password = generatePassword();
}

DeviceIdentity DeviceIdentity::load() {
    DeviceIdentity id;
    const std::string dir = getConfigDir();
    const std::string path = dir +
#if defined(_WIN32)
        "\\device.conf";
#else
        "/device.conf";
#endif

    std::ifstream in(path);
    if (in.is_open()) {
        std::getline(in, id.deviceId);
        in.close();
    }

    if (id.deviceId.empty() || id.deviceId.size() != 9) {
        id.deviceId = generateDeviceId();
        ensureDir(dir);
        std::ofstream out(path);
        if (out.is_open()) {
            out << id.deviceId << "\n";
        }
    }

    id.password = generatePassword();
    return id;
}

}  // namespace rd
