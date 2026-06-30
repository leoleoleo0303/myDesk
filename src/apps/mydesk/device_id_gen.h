#pragma once

#include <cstdint>
#include <string>

namespace rd {

// Device ID + password, like ToDesk's pairing model.
// Device ID is persistent (saved to config), password regenerates on each launch.
struct DeviceIdentity {
    std::string deviceId;   // 9-digit number, e.g. "123456789"
    std::string password;   // 6-char alphanumeric

    // Load from config (generates and saves if not existing)
    static DeviceIdentity load();

    // Regenerate password (keeps device ID)
    void regeneratePassword();
};

std::string generateDeviceId();
std::string generatePassword();

}  // namespace rd
