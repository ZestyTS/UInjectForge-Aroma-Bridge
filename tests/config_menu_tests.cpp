#include <atomic>
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct ConfigItemBoolean {};
struct ConfigItemIntegerRange {};
using WUPSConfigCategoryHandle = int;
using WUPSConfigAPICallbackStatus = int;
constexpr int WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS = 0;
constexpr int kDefaultBridgePort = 8574, kMinPort = 1, kMaxPort = 65535;
#define UIF_COMPONENT_VERSION "1.0.6-dev"
#define UIF_COMPONENT_BUILD_ID "menu-test"
static std::atomic<bool> sEnabled{true}, sSocketReady{true}, sPassiveBeacon{false};
static std::atomic<int> sBridgePort{kDefaultBridgePort};

struct Node {
    std::string label, key;
    int value = 0, defaultValue = 0, minimum = 0, maximum = 0;
    std::vector<Node> children;
};
struct WUPSConfigItemStub {
    static Node Create(const std::string &label) { return {label, "", 0, 0, 0, 0, {}}; }
};
struct WUPSConfigItemBoolean {
    static Node Create(const char *key, const char *label, bool fallback, bool value, void (*)(ConfigItemBoolean *, bool)) {
        return {label, key, value, fallback, 0, 1, {}};
    }
};
struct WUPSConfigItemIntegerRange {
    static Node Create(const char *key, const char *label, int fallback, int value, int minimum, int maximum,
                       void (*)(ConfigItemIntegerRange *, int32_t)) {
        return {label, key, value, fallback, minimum, maximum, {}};
    }
};
static Node menu;
class WUPSConfigCategory {
    std::shared_ptr<Node> owned;
    Node *node;
public:
    explicit WUPSConfigCategory(int) : node(&menu) {}
    explicit WUPSConfigCategory(const char *label) : owned(std::make_shared<Node>()), node(owned.get()) { node->label = label; }
    static WUPSConfigCategory Create(const char *label) { return WUPSConfigCategory(label); }
    void add(Node item) { node->children.push_back(std::move(item)); }
    void add(WUPSConfigCategory &&category) { node->children.push_back(std::move(*category.node)); }
};
static void enabled_changed(ConfigItemBoolean *, bool) {}
static void passive_changed(ConfigItemBoolean *, bool) {}
static void port_changed(ConfigItemIntegerRange *, int32_t) {}
static void gamepad_config_opened(WUPSConfigCategory &) {}
static std::string ip_address_string() { return "192.168.1.50"; }
#include "readiness_menu.inc"

int main() {
    assert(config_opened(1) == WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS);
    assert(menu.children.size() == 5);
    assert(menu.children[0].key == "enabled" && menu.children[0].value == 1);
    assert(menu.children[1].label == "Console 192.168.1.50 - ready");
    auto &advanced = menu.children[3];
    assert(advanced.label == "Advanced network settings" && advanced.children.size() == 2);
    assert(advanced.children[0].key == "bridgePort" && advanced.children[0].defaultValue == 8574);
    assert(advanced.children[0].minimum == 1 && advanced.children[0].maximum == 65535);
    assert(advanced.children[1].key == "passiveBeacon" && advanced.children[1].defaultValue == 0);
    assert(menu.children[4].label == "Diagnostics" && menu.children[4].children.size() == 2);
    assert(menu.children[4].children[0].label.find("menu-test") != std::string::npos);
    assert(menu.children[4].children[1].label == "Checking SD...");
    menu = {}; sEnabled = false;
    config_opened(1);
    assert(menu.children[1].label.find("discovery off") != std::string::npos);
    menu = {}; sEnabled = true; sSocketReady = false;
    config_opened(1);
    assert(menu.children[1].label.find("discovery unavailable") != std::string::npos);
    std::puts("Bridge production menu: compact root, advanced settings, diagnostics and disabled/offline states passed.");
}
