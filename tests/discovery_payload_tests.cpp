#include <atomic>
#include <cassert>
#include <climits>
#include <cstdio>
#include <string>

#define UIF_COMPONENT_VERSION "1.0.6-dev"
#define UIF_COMPONENT_BUILD_ID "src-0123456789ab"
#define BRIDGE_CAPABILITIES_JSON "[\"discovery\",\"setup-check\",\"status-file\"]"
#define BRIDGE_TYPE "uinjectforge-aroma-bridge"
#define BRIDGE_DEVICE_NAME "UInjectForge Aroma Bridge"
#define BRIDGE_PROTOCOL "uif-aroma-bridge-v1"
static constexpr size_t kMaxDiscoveryPayloadBytes = 1280;
static constexpr int kDefaultAssistantPort = 8573, kDefaultFtpPort = 21;
static constexpr const char *kStatusPath = "fs:/vol/external01/uinjectforge/aroma-bridge/status.json";
static std::atomic<int> sBridgePort{65535};
static std::atomic<bool> sPassiveBeacon{false}, sSocketReady{true};
static std::string ip_address_string() { return "192.168.255.255"; }
#include "discovery_payload.inc"

static void check(const BridgeReadiness &readiness) {
    const auto json = build_beacon_json(readiness);
    assert(!json.empty() && json.size() <= kMaxDiscoveryPayloadBytes);
    assert(json.front() == '{' && json.back() == '}');
    assert(json.find("\"setup\":{\"schemaVersion\":1") != std::string::npos);
    assert(json.find("\"setupSummary\"") == std::string::npos);
    assert(json.find("\"probeCount\"") == std::string::npos);
    std::printf("%s\n", json.c_str());
}

int main() {
    BridgeReadiness readiness{};
    check(readiness);
    readiness.checked = true;
    check(readiness);
    readiness.sdAccessible = true;
    readiness.sdSpaceKnown = true;
    readiness.sdFreeBytes = LLONG_MAX;
    readiness.validInstallPackages = 512;
    readiness.invalidInstallPackages = 512;
    readiness.installScanTruncated = true;
    check(readiness);
    readiness.assistantInstalled = readiness.aromaBridgeInstalled = readiness.ftpPluginInstalled = true;
    readiness.nintendontInstalled = readiness.noodsInstalled = readiness.ppssppRuntimeInstalled = true;
    readiness.ppssppAssetsComplete = readiness.wiiStationInstalled = readiness.not64Installed = true;
    readiness.genesisPlusGxInstalled = readiness.controllerConfigInstalled = true;
    readiness.loadiineFolderPresent = readiness.installFolderPresent = true;
    sPassiveBeacon = true;
    check(readiness);
}
