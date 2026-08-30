#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include <arpa/inet.h>
#include <coreinit/debug.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <fcntl.h>
#include <nn/ac.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <wups.h>
#include <wups/config/WUPSConfigCategory.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemIntegerRange.h>
#include <wups/config/WUPSConfigItemStub.h>

#define BRIDGE_NAME "UInjectForge Bridge"
#define BRIDGE_DEVICE_NAME "UInjectForge Aroma Bridge"
#define BRIDGE_PROTOCOL "uif-aroma-bridge-v1"
#ifndef UIF_COMPONENT_VERSION
#define UIF_COMPONENT_VERSION "1.0.0-alpha"
#endif
#ifndef UIF_COMPONENT_BUILD_ID
#define UIF_COMPONENT_BUILD_ID "dev"
#endif
#define BRIDGE_CAPABILITIES_JSON "[\"discovery\",\"setup-check\",\"status-file\"]"
#define BRIDGE_TYPE "uinjectforge-aroma-bridge"
#define ASSISTANT_DISCOVERY_REQUEST "uinjectforge-assistant-discovery-request"
#define BRIDGE_DISCOVERY_REQUEST "uinjectforge-aroma-bridge-discovery-request"

WUPS_PLUGIN_NAME(BRIDGE_NAME);
WUPS_PLUGIN_DESCRIPTION("Setup and discovery bridge for UInjectForge");
WUPS_PLUGIN_VERSION(UIF_COMPONENT_VERSION);
WUPS_PLUGIN_AUTHOR("ZestyTS");
WUPS_PLUGIN_LICENSE("MIT");

WUPS_USE_STORAGE("uinjectforge-aroma-bridge");

static constexpr int32_t kDefaultBridgePort = 8574;
static constexpr int32_t kDefaultAssistantPort = 8573;
static constexpr int32_t kDefaultFtpPort = 21;
static constexpr int32_t kMinPort = 1;
static constexpr int32_t kMaxPort = 65535;
static constexpr int32_t kMaxSetupPackages = 512;
static constexpr uint32_t kReadinessRefreshIntervalMs = 5U * 60U * 1000U;
static constexpr uint32_t kStatusWriteIntervalTicks = 3000U;
static constexpr const char *kSdRoot = "fs:/vol/external01";
static constexpr const char *kStatusDirectory = "fs:/vol/external01/uinjectforge/aroma-bridge";
static constexpr const char *kStatusPath = "fs:/vol/external01/uinjectforge/aroma-bridge/status.json";
static constexpr const char *kStatusTempPath = "fs:/vol/external01/uinjectforge/aroma-bridge/status.json.tmp";

static constexpr const char *kPpssppRequiredAssetPaths[] = {
    "flash0/font/ltn0.pgf",
    "flash0/font/jpn0.pgf",
    "flash0/font/kr0.pgf",
    "flash0/font/ltn1.pgf",
    "flash0/font/ltn2.pgf",
    "flash0/font/ltn3.pgf",
    "flash0/font/ltn4.pgf",
    "flash0/font/ltn5.pgf",
    "flash0/font/ltn6.pgf",
    "flash0/font/ltn7.pgf",
    "flash0/font/ltn8.pgf",
    "flash0/font/ltn9.pgf",
    "flash0/font/ltn10.pgf",
    "flash0/font/ltn11.pgf",
    "flash0/font/ltn12.pgf",
    "flash0/font/ltn13.pgf",
    "flash0/font/ltn14.pgf",
    "flash0/font/ltn15.pgf",
    "ui_atlas.zim",
    "ui_atlas.meta",
    "ppge_atlas.zim",
    "ppge_atlas.meta",
    "Roboto-Condensed.ttf",
    "compat.ini",
    "langregion.ini",
    "lang/en_US.ini",
    "shaders/defaultshaders.ini",
};
static std::atomic<bool> sEnabled{true};
static std::atomic<bool> sPassiveBeacon{false};
static std::atomic<bool> sRunning{false};
static std::atomic<bool> sSocketReady{false};
static std::atomic<int32_t> sBridgePort{kDefaultBridgePort};
static std::atomic<uint32_t> sProbeCount{0};
static std::atomic<uint32_t> sReplyCount{0};
static std::atomic<uint32_t> sBeaconCount{0};
static std::thread sWorker;
static std::mutex sStateMutex;
static std::mutex sReadinessMutex;
static std::mutex sStatusMutex;
static std::string sLastPeer = "none";

struct BridgeReadiness {
    bool sdAccessible = false;
    bool sdSpaceKnown = false;
    long long sdFreeBytes = 0;
    bool assistantInstalled = false;
    bool aromaBridgeInstalled = false;
    bool ftpPluginInstalled = false;
    bool nintendontInstalled = false;
    bool noodsInstalled = false;
    bool ppssppRuntimeInstalled = false;
    bool ppssppAssetsComplete = false;
    bool wiiStationInstalled = false;
    bool not64Installed = false;
    bool genesisPlusGxInstalled = false;
    bool controllerConfigInstalled = false;
    bool loadiineFolderPresent = false;
    bool installFolderPresent = false;
    int validInstallPackages = 0;
    int invalidInstallPackages = 0;
    bool installScanTruncated = false;
};

static BridgeReadiness sCachedReadiness{};
static bool sCachedReadinessValid = false;
static uint64_t sCachedReadinessTicks = 0;
static std::string sLastStatusContent;

static void set_last_peer(const char *peer) {
    std::lock_guard<std::mutex> guard(sStateMutex);
    sLastPeer = peer == nullptr || peer[0] == '\0' ? "unknown" : peer;
}

static std::string last_peer() {
    std::lock_guard<std::mutex> guard(sStateMutex);
    return sLastPeer;
}

static std::string ip_address_string() {
    uint32_t hostIpAddress = 0;
    nn::ac::GetAssignedAddress(&hostIpAddress);
    if (hostIpAddress == 0) {
        return "unknown";
    }

    char buffer[32];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%u.%u.%u.%u",
                  (hostIpAddress >> 24) & 0xFF,
                  (hostIpAddress >> 16) & 0xFF,
                  (hostIpAddress >> 8) & 0xFF,
                  hostIpAddress & 0xFF);
    return buffer;
}

static bool file_exists(const char *path) {
    FILE *file = std::fopen(path, "rb");
    if (!file) {
        return false;
    }

    std::fclose(file);
    return true;
}

static bool ppsspp_assets_complete(const char *assetsRoot) {
    for (const char *asset : kPpssppRequiredAssetPaths) {
        char path[896];
        std::snprintf(path, sizeof(path), "%s/%s", assetsRoot, asset);
        if (!file_exists(path)) {
            return false;
        }
    }

    return true;
}
static bool directory_exists(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        return false;
    }

    closedir(dir);
    return true;
}

static bool ends_with_ignore_case(const char *value, const char *suffix) {
    if (!value || !suffix) {
        return false;
    }

    const size_t valueLength = std::strlen(value);
    const size_t suffixLength = std::strlen(suffix);
    if (valueLength < suffixLength) {
        return false;
    }

    const char *tail = value + valueLength - suffixLength;
    for (size_t i = 0; i < suffixLength; i++) {
        if (std::tolower(static_cast<unsigned char>(tail[i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i]))) {
            return false;
        }
    }

    return true;
}

static int count_files_with_suffix(const char *path, const char *suffix) {
    DIR *dir = opendir(path);
    if (!dir) {
        return 0;
    }

    int count = 0;
    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] != '.' && ends_with_ignore_case(entry->d_name, suffix)) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

static bool directory_has_ini(const char *path) {
    return count_files_with_suffix(path, ".ini") > 0;
}

static void inspect_install_packages(BridgeReadiness &result) {
    constexpr const char *installRoot = "fs:/vol/external01/install";
    DIR *dir = opendir(installRoot);
    if (!dir) {
        return;
    }

    result.installFolderPresent = true;
    int inspected = 0;
    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        char packagePath[768];
        std::snprintf(packagePath, sizeof(packagePath), "%s/%s", installRoot, entry->d_name);
        DIR *packageDir = opendir(packagePath);
        if (!packageDir) {
            continue;
        }
        closedir(packageDir);

        if (inspected >= kMaxSetupPackages) {
            result.installScanTruncated = true;
            break;
        }
        inspected++;

        char ticketPath[896];
        char tmdPath[896];
        char certPath[896];
        std::snprintf(ticketPath, sizeof(ticketPath), "%s/title.tik", packagePath);
        std::snprintf(tmdPath, sizeof(tmdPath), "%s/title.tmd", packagePath);
        std::snprintf(certPath, sizeof(certPath), "%s/title.cert", packagePath);
        const bool valid = file_exists(ticketPath) && file_exists(tmdPath) && file_exists(certPath) &&
                           count_files_with_suffix(packagePath, ".app") > 0;
        if (valid) {
            result.validInstallPackages++;
        } else {
            result.invalidInstallPackages++;
        }
    }

    closedir(dir);
}

static BridgeReadiness inspect_readiness() {
    BridgeReadiness result{};
    result.sdAccessible = directory_exists(kSdRoot);
    if (!result.sdAccessible) {
        return result;
    }

    result.assistantInstalled =
        file_exists("fs:/vol/external01/wiiu/apps/uinjectforge-assistant/uinjectforge-assistant.rpx") ||
        file_exists("fs:/vol/external01/wiiu/apps/uinjectforge-assistant/uinjectforge-assistant.wuhb");
    result.aromaBridgeInstalled =
        file_exists("fs:/vol/external01/wiiu/environments/aroma/plugins/uinjectforge-aroma-bridge.wps");
    result.ftpPluginInstalled =
        file_exists("fs:/vol/external01/wiiu/environments/aroma/plugins/ftpiiu_plugin.wps") ||
        file_exists("fs:/vol/external01/wiiu/environments/aroma/plugins/ftpiiu.wps") ||
        file_exists("fs:/vol/external01/wiiu/environments/aroma/plugins/ftpiiu_everywhere.wps");
    result.nintendontInstalled =
        file_exists("fs:/vol/external01/apps/nintendont/boot.dol") ||
        file_exists("fs:/vol/external01/apps/Nintendont/boot.dol");
    result.noodsInstalled =
        file_exists("fs:/vol/external01/wiiu/apps/noods/noods.rpx") ||
        file_exists("fs:/vol/external01/wiiu/apps/noods/noods.wuhb") ||
        file_exists("fs:/vol/external01/wiiu/apps/NooDS/noods.rpx") ||
        file_exists("fs:/vol/external01/wiiu/apps/NooDS/noods.wuhb");
    result.ppssppRuntimeInstalled =
        file_exists("fs:/vol/external01/wiiu/apps/ppsspp/PPSSPP.rpx") ||
        file_exists("fs:/vol/external01/wiiu/apps/ppsspp/ppsspp.rpx") ||
        file_exists("fs:/vol/external01/ppsspp/PPSSPP.rpx") ||
        file_exists("fs:/vol/external01/ppsspp/ppsspp.rpx");
    result.ppssppAssetsComplete =
        ppsspp_assets_complete("fs:/vol/external01/wiiu/apps/ppsspp/assets") ||
        ppsspp_assets_complete("fs:/vol/external01/ppsspp/assets");
    result.wiiStationInstalled =
        file_exists("fs:/vol/external01/apps/WiiStation/boot.dol") ||
        file_exists("fs:/vol/external01/apps/wiistation/boot.dol");
    result.not64Installed =
        ((file_exists("fs:/vol/external01/apps/not64/boot.dol") &&
          file_exists("fs:/vol/external01/apps/not64/data.iso")) ||
         (file_exists("fs:/vol/external01/apps/Not64/boot.dol") &&
          file_exists("fs:/vol/external01/apps/Not64/data.iso")));
    result.genesisPlusGxInstalled =
        file_exists("fs:/vol/external01/apps/genplus-gx/boot.dol") ||
        file_exists("fs:/vol/external01/apps/genplusgx/boot.dol");
    result.controllerConfigInstalled =
        file_exists("fs:/vol/external01/controller.ini") ||
        file_exists("fs:/vol/external01/apps/nintendont/controller.ini") ||
        file_exists("fs:/vol/external01/apps/Nintendont/controller.ini") ||
        directory_has_ini("fs:/vol/external01/controllers");
    result.loadiineFolderPresent = directory_exists("fs:/vol/external01/wiiu/games");

    struct statvfs spaceInfo {};
    if (statvfs(kSdRoot, &spaceInfo) == 0) {
        result.sdSpaceKnown = true;
        result.sdFreeBytes = static_cast<long long>(spaceInfo.f_bavail) * static_cast<long long>(spaceInfo.f_frsize);
    }

    inspect_install_packages(result);
    return result;
}

static BridgeReadiness readiness_snapshot(bool forceRefresh) {
    std::lock_guard<std::mutex> guard(sReadinessMutex);
    const auto now = OSGetTime();
    const auto refreshInterval = OSMillisecondsToTicks(kReadinessRefreshIntervalMs);
    if (!sCachedReadinessValid || forceRefresh || now - sCachedReadinessTicks >= refreshInterval) {
        sCachedReadiness = inspect_readiness();
        sCachedReadinessTicks = now;
        sCachedReadinessValid = true;
    }

    return sCachedReadiness;
}

static std::string readiness_summary(const BridgeReadiness &readiness) {
    if (!readiness.sdAccessible) {
        return "SD not accessible";
    }

    char buffer[512];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "SD ready; Assistant %s; FTP %s; Nintendont %s; NooDS %s; PPSSPP %s; vWii presets %d/3; %d valid and %d incomplete install package(s)",
                  readiness.assistantInstalled ? "found" : "missing",
                  readiness.ftpPluginInstalled ? "found" : "missing",
                  readiness.nintendontInstalled ? "found" : "missing",
                  readiness.noodsInstalled ? "found" : "missing",
                  readiness.ppssppRuntimeInstalled && readiness.ppssppAssetsComplete ? "ready" : readiness.ppssppRuntimeInstalled ? "assets incomplete" : "missing",
                  (readiness.wiiStationInstalled ? 1 : 0) + (readiness.not64Installed ? 1 : 0) + (readiness.genesisPlusGxInstalled ? 1 : 0),
                  readiness.validInstallPackages,
                  readiness.invalidInstallPackages);
    return buffer;
}

static std::string readiness_json(const BridgeReadiness &readiness, const std::string &summary) {
    char buffer[2048];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "{\"schemaVersion\":1,\"sdAccessible\":%s,\"sdSpaceKnown\":%s,\"sdFreeBytes\":%lld,"
        "\"assistantInstalled\":%s,\"aromaBridgeInstalled\":%s,\"ftpPluginInstalled\":%s,"
        "\"nintendontInstalled\":%s,\"noodsInstalled\":%s,\"ppssppRuntimeInstalled\":%s,"
        "\"ppssppAssetsComplete\":%s,\"wiiStationInstalled\":%s,\"not64Installed\":%s,"
        "\"genesisPlusGxInstalled\":%s,\"controllerConfigInstalled\":%s,\"loadiineFolderPresent\":%s,"
        "\"installFolderPresent\":%s,\"validInstallPackages\":%d,\"invalidInstallPackages\":%d,"
        "\"installScanTruncated\":%s,\"summary\":\"%s\"}",
        readiness.sdAccessible ? "true" : "false",
        readiness.sdSpaceKnown ? "true" : "false",
        readiness.sdFreeBytes,
        readiness.assistantInstalled ? "true" : "false",
        readiness.aromaBridgeInstalled ? "true" : "false",
        readiness.ftpPluginInstalled ? "true" : "false",
        readiness.nintendontInstalled ? "true" : "false",
        readiness.noodsInstalled ? "true" : "false",
        readiness.ppssppRuntimeInstalled ? "true" : "false",
        readiness.ppssppAssetsComplete ? "true" : "false",
        readiness.wiiStationInstalled ? "true" : "false",
        readiness.not64Installed ? "true" : "false",
        readiness.genesisPlusGxInstalled ? "true" : "false",
        readiness.controllerConfigInstalled ? "true" : "false",
        readiness.loadiineFolderPresent ? "true" : "false",
        readiness.installFolderPresent ? "true" : "false",
        readiness.validInstallPackages,
        readiness.invalidInstallPackages,
        readiness.installScanTruncated ? "true" : "false",
        summary.c_str());
    return buffer;
}

static std::string build_beacon_json(const BridgeReadiness &readiness) {
    char buffer[4096];
    const auto peer = last_peer();
    const auto setupSummary = readiness_summary(readiness);
    const auto setupJson = readiness_json(readiness, setupSummary);
    std::snprintf(buffer,
                  sizeof(buffer),
                  "{\"type\":\"%s\",\"deviceName\":\"%s\",\"protocolVersion\":\"%s\","
                  "\"componentVersion\":\"" UIF_COMPONENT_VERSION "\",\"buildId\":\"" UIF_COMPONENT_BUILD_ID "\","
                  "\"capabilities\":" BRIDGE_CAPABILITIES_JSON ","
                  "\"ipAddress\":\"%s\",\"port\":%d,\"assistantPort\":%d,\"ftpPort\":%d,"
                  "\"paired\":false,\"activeTransfer\":false,"
                  "\"passiveBroadcastEnabled\":%s,\"simpleUi\":false,"
                  "\"broadcastPort\":%d,\"broadcastSocketReady\":%s,"
                  "\"sdAccessible\":%s,\"assistantInstalled\":%s,"
                  "\"aromaBridgeInstalled\":%s,\"ftpPluginInstalled\":%s,"
                  "\"nintendontInstalled\":%s,\"installPackageCount\":%d,"
                  "\"statusFilePath\":\"%s\",\"setupSummary\":\"%s\","
                  "\"setup\":%s,"
                  "\"probeCount\":%u,\"replyCount\":%u,\"beaconCount\":%u,"
                  "\"lastDiscoveryPeer\":\"%s\"}",
                  BRIDGE_TYPE,
                  BRIDGE_DEVICE_NAME,
                  BRIDGE_PROTOCOL,
                  ip_address_string().c_str(),
                  static_cast<int>(sBridgePort.load()),
                  kDefaultAssistantPort,
                  kDefaultFtpPort,
                  sPassiveBeacon.load() ? "true" : "false",
                  static_cast<int>(sBridgePort.load()),
                  sSocketReady.load() ? "true" : "false",
                  readiness.sdAccessible ? "true" : "false",
                  readiness.assistantInstalled ? "true" : "false",
                  readiness.aromaBridgeInstalled ? "true" : "false",
                  readiness.ftpPluginInstalled ? "true" : "false",
                  readiness.nintendontInstalled ? "true" : "false",
                  readiness.validInstallPackages,
                  kStatusPath,
                  setupSummary.c_str(),
                  setupJson.c_str(),
                  static_cast<unsigned>(sProbeCount.load()),
                  static_cast<unsigned>(sReplyCount.load()),
                  static_cast<unsigned>(sBeaconCount.load()),
                  peer.c_str());
    return buffer;
}

static std::string build_status_json(const BridgeReadiness &readiness) {
    char buffer[4096];
    const auto setupSummary = readiness_summary(readiness);
    const auto setupJson = readiness_json(readiness, setupSummary);
    std::snprintf(buffer,
                  sizeof(buffer),
                  "{\n"
                  "  \"component\": \"UInjectForge Aroma Bridge\",\n"
                  "  \"protocolVersion\": \"%s\",\n"
                  "  \"componentVersion\": \"" UIF_COMPONENT_VERSION "\",\n"
                  "  \"buildId\": \"" UIF_COMPONENT_BUILD_ID "\",\n"
                  "  \"capabilities\": " BRIDGE_CAPABILITIES_JSON ",\n"
                  "  \"ipAddress\": \"%s\",\n"
                  "  \"bridgePort\": %d,\n"
                  "  \"assistantPort\": %d,\n"
                  "  \"ftpPort\": %d,\n"
                  "  \"sdAccessible\": true,\n"
                  "  \"sdSpaceKnown\": %s,\n"
                  "  \"sdFreeBytes\": %lld,\n"
                  "  \"assistantInstalled\": %s,\n"
                  "  \"aromaBridgeInstalled\": %s,\n"
                  "  \"ftpPluginInstalled\": %s,\n"
                  "  \"nintendontInstalled\": %s,\n"
                  "  \"noodsInstalled\": %s,\n"
                  "  \"ppssppRuntimeInstalled\": %s,\n"
                  "  \"ppssppAssetsComplete\": %s,\n"
                  "  \"wiiStationInstalled\": %s,\n"
                  "  \"not64Installed\": %s,\n"
                  "  \"genesisPlusGxInstalled\": %s,\n"
                  "  \"controllerConfigInstalled\": %s,\n"
                  "  \"loadiineFolderPresent\": %s,\n"
                  "  \"installFolderPresent\": %s,\n"
                  "  \"validInstallPackages\": %d,\n"
                  "  \"invalidInstallPackages\": %d,\n"
                  "  \"installScanTruncated\": %s,\n"
                  "  \"probeCount\": %u,\n"
                  "  \"replyCount\": %u,\n"
                  "  \"beaconCount\": %u,\n"
                  "  \"lastDiscoveryPeer\": \"%s\",\n"
                  "  \"setupSummary\": \"%s\",\n"
                  "  \"setup\": %s\n"
                  "}\n",
                  BRIDGE_PROTOCOL,
                  ip_address_string().c_str(),
                  static_cast<int>(sBridgePort.load()),
                  kDefaultAssistantPort,
                  kDefaultFtpPort,
                  readiness.sdSpaceKnown ? "true" : "false",
                  readiness.sdFreeBytes,
                  readiness.assistantInstalled ? "true" : "false",
                  readiness.aromaBridgeInstalled ? "true" : "false",
                  readiness.ftpPluginInstalled ? "true" : "false",
                  readiness.nintendontInstalled ? "true" : "false",
                  readiness.noodsInstalled ? "true" : "false",
                  readiness.ppssppRuntimeInstalled ? "true" : "false",
                  readiness.ppssppAssetsComplete ? "true" : "false",
                  readiness.wiiStationInstalled ? "true" : "false",
                  readiness.not64Installed ? "true" : "false",
                  readiness.genesisPlusGxInstalled ? "true" : "false",
                  readiness.controllerConfigInstalled ? "true" : "false",
                  readiness.loadiineFolderPresent ? "true" : "false",
                  readiness.installFolderPresent ? "true" : "false",
                  readiness.validInstallPackages,
                  readiness.invalidInstallPackages,
                  readiness.installScanTruncated ? "true" : "false",
                  static_cast<unsigned>(sProbeCount.load()),
                  static_cast<unsigned>(sReplyCount.load()),
                  static_cast<unsigned>(sBeaconCount.load()),
                  last_peer().c_str(),
                  setupSummary.c_str(),
                  setupJson.c_str());
    return buffer;
}

static void write_status_file(const BridgeReadiness &readiness) {
    if (!readiness.sdAccessible) {
        return;
    }

    const auto status = build_status_json(readiness);
    std::lock_guard<std::mutex> guard(sStatusMutex);
    if (status == sLastStatusContent && file_exists(kStatusPath)) {
        return;
    }

    mkdir("fs:/vol/external01/uinjectforge", 0777);
    mkdir(kStatusDirectory, 0777);

    FILE *file = std::fopen(kStatusTempPath, "wb");
    if (!file) {
        return;
    }

    const auto written = std::fwrite(status.data(), 1, status.size(), file);
    const bool flushed = written == status.size() && std::fflush(file) == 0;
    if (flushed) {
        const auto descriptor = fileno(file);
        if (descriptor >= 0) {
            (void)fsync(descriptor);
        }
    }
    const bool closed = std::fclose(file) == 0;
    if (!flushed || !closed) {
        remove(kStatusTempPath);
        return;
    }

    if (rename(kStatusTempPath, kStatusPath) != 0) {
        remove(kStatusPath);
        if (rename(kStatusTempPath, kStatusPath) != 0) {
            remove(kStatusTempPath);
            return;
        }
    }

    sLastStatusContent = status;
}

static bool is_discovery_probe(const char *message, ssize_t length) {
    if (message == nullptr || length <= 0) {
        return false;
    }

    std::string_view view(message, static_cast<size_t>(length));
    return view.find(ASSISTANT_DISCOVERY_REQUEST) != std::string_view::npos ||
           view.find(BRIDGE_DISCOVERY_REQUEST) != std::string_view::npos;
}

static void send_beacon(int socketFd, const sockaddr_in &target, const BridgeReadiness &readiness) {
    const auto json = build_beacon_json(readiness);
    if (sendto(socketFd, json.c_str(), json.size(), 0, reinterpret_cast<const sockaddr *>(&target), sizeof(target)) >= 0) {
        sReplyCount++;
    }
}

static void send_passive_beacon(int socketFd) {
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(static_cast<uint16_t>(sBridgePort.load()));
    target.sin_addr.s_addr = INADDR_BROADCAST;
    const auto json = build_beacon_json(readiness_snapshot(false));
    if (sendto(socketFd, json.c_str(), json.size(), 0, reinterpret_cast<const sockaddr *>(&target), sizeof(target)) >= 0) {
        sBeaconCount++;
    }
}

static void discovery_loop() {
    nn::ac::Initialize();
    nn::ac::ConnectAsync();

    const auto initialReadiness = readiness_snapshot(true);
    write_status_file(initialReadiness);

    int socketFd = -1;
    int currentPort = 0;
    uint32_t idleTicks = 0;
    uint32_t statusTicks = 0;

    while (sRunning.load()) {
        const auto enabled = sEnabled.load();
        const auto wantedPort = sBridgePort.load();

        if (!enabled) {
            if (socketFd >= 0) {
                close(socketFd);
                socketFd = -1;
                currentPort = 0;
                sSocketReady = false;
            }
            OSSleepTicks(OSMillisecondsToTicks(250));
            continue;
        }

        if (socketFd < 0 || currentPort != wantedPort) {
            if (socketFd >= 0) {
                close(socketFd);
                socketFd = -1;
                sSocketReady = false;
            }

            socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (socketFd < 0) {
                OSSleepTicks(OSMillisecondsToTicks(500));
                continue;
            }

            int yes = 1;
            setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
            setsockopt(socketFd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
            fcntl(socketFd, F_SETFL, O_NONBLOCK);

            sockaddr_in local{};
            local.sin_family = AF_INET;
            local.sin_addr.s_addr = INADDR_ANY;
            local.sin_port = htons(static_cast<uint16_t>(wantedPort));

            if (bind(socketFd, reinterpret_cast<sockaddr *>(&local), sizeof(local)) < 0) {
                OSReport("UInjectForge Bridge: failed to bind UDP %d, errno %d\n", static_cast<int>(wantedPort), errno);
                close(socketFd);
                socketFd = -1;
                sSocketReady = false;
                OSSleepTicks(OSMillisecondsToTicks(1000));
                continue;
            }

            currentPort = wantedPort;
            sSocketReady = true;
            OSReport("UInjectForge Bridge: listening on UDP %d\n", currentPort);
        }

        char buffer[512];
        sockaddr_in remote{};
        socklen_t remoteSize = sizeof(remote);
        const auto received = recvfrom(socketFd, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr *>(&remote), &remoteSize);
        if (received > 0) {
            buffer[received] = '\0';
            char peer[32];
            std::snprintf(peer,
                          sizeof(peer),
                          "%u.%u.%u.%u",
                          ntohl(remote.sin_addr.s_addr) >> 24 & 0xFF,
                          ntohl(remote.sin_addr.s_addr) >> 16 & 0xFF,
                          ntohl(remote.sin_addr.s_addr) >> 8 & 0xFF,
                          ntohl(remote.sin_addr.s_addr) & 0xFF);
            set_last_peer(peer);
            if (is_discovery_probe(buffer, received)) {
                sProbeCount++;
                const auto readiness = readiness_snapshot(true);
                send_beacon(socketFd, remote, readiness);
                write_status_file(readiness);
            }
            idleTicks = 0;
            continue;
        }

        if (++statusTicks >= kStatusWriteIntervalTicks) {
            statusTicks = 0;
            write_status_file(readiness_snapshot(true));
        }

        if (sPassiveBeacon.load() && ++idleTicks >= 50) {
            idleTicks = 0;
            send_passive_beacon(socketFd);
        }

        OSSleepTicks(OSMillisecondsToTicks(100));
    }

    if (socketFd >= 0) {
        close(socketFd);
    }
    sSocketReady = false;
}

static void restart_worker() {
    if (sRunning.load()) {
        return;
    }

    sRunning = true;
    sWorker = std::thread(discovery_loop);
    auto *native = reinterpret_cast<OSThread *>(sWorker.native_handle());
    OSSetThreadName(native, "uif-aroma-bridge");
    OSSetThreadAffinity(native, OS_THREAD_ATTRIB_AFFINITY_CPU2);
    OSSetThreadPriority(native, 24);
}

static void stop_worker() {
    if (!sRunning.exchange(false)) {
        return;
    }

    if (sWorker.joinable()) {
        sWorker.join();
    }
}

static void enabled_changed(ConfigItemBoolean *, bool newValue) {
    sEnabled = newValue;
    WUPSStorageAPI::Store("enabled", newValue);
}

static void port_changed(ConfigItemIntegerRange *, int32_t newValue) {
    if (newValue < kMinPort || newValue > kMaxPort) {
        return;
    }

    sBridgePort = newValue;
    WUPSStorageAPI::Store("bridgePort", newValue);
}

static void passive_changed(ConfigItemBoolean *, bool newValue) {
    sPassiveBeacon = newValue;
    WUPSStorageAPI::Store("passiveBeacon", newValue);
}

static WUPSConfigAPICallbackStatus config_opened(WUPSConfigCategoryHandle rootHandle) {
    WUPSConfigCategory root(rootHandle);
    root.add(WUPSConfigItemBoolean::Create("enabled",
                                           "Enable bridge discovery",
                                           true,
                                           sEnabled.load(),
                                           enabled_changed));
    root.add(WUPSConfigItemIntegerRange::Create("bridgePort",
                                                "Discovery UDP port",
                                                kDefaultBridgePort,
                                                sBridgePort.load(),
                                                kMinPort,
                                                kMaxPort,
                                                port_changed));
    root.add(WUPSConfigItemBoolean::Create("passiveBeacon",
                                           "Passive discovery beacon",
                                           false,
                                           sPassiveBeacon.load(),
                                           passive_changed));

    char status[160];
    std::snprintf(status,
                  sizeof(status),
                  "IP %s, UDP %d, %s",
                  ip_address_string().c_str(),
                  static_cast<int>(sBridgePort.load()),
                  sSocketReady.load() ? "listening" : "not listening");
    root.add(WUPSConfigItemStub::Create(status));
    char component[160];
    std::snprintf(component, sizeof(component), "Version %s | Build %s", UIF_COMPONENT_VERSION, UIF_COMPONENT_BUILD_ID);
    root.add(WUPSConfigItemStub::Create(component));
    const auto readiness = readiness_snapshot(true);
    write_status_file(readiness);
    root.add(WUPSConfigItemStub::Create(readiness_summary(readiness)));
    root.add(WUPSConfigItemStub::Create("UIF fills host fields and setup status from this bridge."));
    root.add(WUPSConfigItemStub::Create("Transfers still use Assistant 8573 or FTPiiU 21."));
    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

static void config_closed() {
    WUPSStorageAPI::SaveStorage();
}

INITIALIZE_PLUGIN() {
    WUPSConfigAPIOptionsV1 configOptions = {.name = BRIDGE_NAME};
    if (WUPSConfigAPI_Init(configOptions, config_opened, config_closed) != WUPSCONFIG_API_RESULT_SUCCESS) {
        OSFatal("UInjectForge Bridge: failed to initialize config API");
    }

    bool enabled = true;
    bool passiveBeacon = false;
    int32_t bridgePort = kDefaultBridgePort;
    WUPSStorageAPI::GetOrStoreDefault("enabled", enabled, true);
    WUPSStorageAPI::GetOrStoreDefault("passiveBeacon", passiveBeacon, false);
    WUPSStorageAPI::GetOrStoreDefault("bridgePort", bridgePort, kDefaultBridgePort);

    if (bridgePort < kMinPort || bridgePort > kMaxPort) {
        bridgePort = kDefaultBridgePort;
    }

    sEnabled = enabled;
    sPassiveBeacon = passiveBeacon;
    sBridgePort = bridgePort;
    WUPSStorageAPI::SaveStorage();
}

DEINITIALIZE_PLUGIN() {
    stop_worker();
}

ON_APPLICATION_START() {
    restart_worker();
}

ON_APPLICATION_ENDS() {
    stop_worker();
}
