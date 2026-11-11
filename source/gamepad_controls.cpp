#include "gamepad_profile.h"
#include <cstdio>
#include <mutex>
#ifdef UIF_GAMEPAD_HOST_TEST
#include "gamepad_sdk_stubs.h"
#else
#include "gamepad_controls.h"
#include <coreinit/debug.h>
#include <coreinit/title.h>
#include <vpad/input.h>
#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>
#include <wups/config/WUPSConfigItemStub.h>
static_assert(uif::gamepad::buttons[0] == VPAD_BUTTON_A && uif::gamepad::buttons[1] == VPAD_BUTTON_B
    && uif::gamepad::buttons[2] == VPAD_BUTTON_X && uif::gamepad::buttons[3] == VPAD_BUTTON_Y
    && uif::gamepad::buttons[4] == VPAD_BUTTON_LEFT && uif::gamepad::buttons[5] == VPAD_BUTTON_RIGHT
    && uif::gamepad::buttons[6] == VPAD_BUTTON_UP && uif::gamepad::buttons[7] == VPAD_BUTTON_DOWN
    && uif::gamepad::buttons[8] == VPAD_BUTTON_ZL && uif::gamepad::buttons[9] == VPAD_BUTTON_ZR
    && uif::gamepad::buttons[10] == VPAD_BUTTON_L && uif::gamepad::buttons[11] == VPAD_BUTTON_R
    && uif::gamepad::buttons[12] == VPAD_BUTTON_PLUS && uif::gamepad::buttons[13] == VPAD_BUTTON_MINUS
    && uif::gamepad::buttons[14] == VPAD_BUTTON_STICK_L && uif::gamepad::buttons[15] == VPAD_BUTTON_STICK_R,
    "Reviewed GamePad masks must match the pinned SDK");
#endif

static std::mutex sGamepadMutex;
static uint64_t sGamepadTitle = 0;
static uif::gamepad::Profile sGamepadProfile, sGamepadPending;
static uif::gamepad::Mapper sGamepadMapper;
static bool sGamepadMenu = false;
static std::string sGamepadError;
enum class GamepadCloseAction : uint32_t { Save, Discard, Restore };
static GamepadCloseAction sGamepadCloseAction = GamepadCloseAction::Save;
static constexpr const char *kGamepadNames[] = {
    "A", "B", "X", "Y", "Left", "Right", "Up", "Down", "ZL", "ZR", "L", "R", "+", "-", "Left stick", "Right stick"};

static std::string gamepad_key(uint64_t title) {
    char key[32];
    std::snprintf(key, sizeof(key), "gamepad.%016llX", static_cast<unsigned long long>(title));
    return key;
}

void gamepad_application_start() {
    std::lock_guard<std::mutex> guard(sGamepadMutex);
    sGamepadTitle = OSGetTitleID();
    sGamepadProfile = {};
    sGamepadPending = {};
    sGamepadCloseAction = GamepadCloseAction::Save;
    sGamepadMenu = false;
    sGamepadError.clear();
    sGamepadMapper.reset();
    if (!uif::gamepad::application_title(sGamepadTitle)) return;
    std::string stored;
    const auto status = WUPSStorageAPI::Get(gamepad_key(sGamepadTitle), stored);
    if (status == WUPS_STORAGE_ERROR_SUCCESS && !uif::gamepad::Profile::decode(stored, sGamepadProfile))
        sGamepadError = "Invalid saved mapping; standard controls retained";
    else if (status != WUPS_STORAGE_ERROR_SUCCESS && status != WUPS_STORAGE_ERROR_NOT_FOUND)
        sGamepadError = "Mapping unavailable; standard controls retained";
    sGamepadMapper.set(sGamepadProfile);
    sGamepadPending = sGamepadProfile;
}

void gamepad_application_end() {
    std::lock_guard<std::mutex> guard(sGamepadMutex);
    sGamepadTitle = 0;
    sGamepadMenu = false;
    sGamepadProfile = sGamepadPending = {};
    sGamepadCloseAction = GamepadCloseAction::Save;
    sGamepadMapper.reset();
}

static void gamepad_enabled_changed(ConfigItemBoolean *, bool enabled) {
    std::lock_guard<std::mutex> guard(sGamepadMutex);
    if (sGamepadMenu) sGamepadPending.enabled = enabled;
}

static void gamepad_destination_changed(ConfigItemMultipleValues *item, uint32_t value) {
    if (!item || !item->identifier || value >= 16) return;
    const std::string_view key(item->identifier);
    if (key.size() != 5 || key.substr(0, 3) != "gp." || key[3] < '0' || key[3] > '1'
        || key[4] < '0' || key[4] > '9') return;
    const auto index = static_cast<size_t>((key[3] - '0') * 10 + key[4] - '0');
    std::lock_guard<std::mutex> guard(sGamepadMutex);
    if (sGamepadMenu && index < 16) sGamepadPending.destinations[index] = static_cast<uint8_t>(value);
}

static void gamepad_close_action_changed(ConfigItemMultipleValues *item, uint32_t value) {
    if (!item || !item->identifier || std::string_view(item->identifier) != "gamepadCloseAction"
        || value > static_cast<uint32_t>(GamepadCloseAction::Restore)) return;
    std::lock_guard<std::mutex> guard(sGamepadMutex);
    if (sGamepadMenu) sGamepadCloseAction = static_cast<GamepadCloseAction>(value);
}

void gamepad_config_opened(WUPSConfigCategory &root) {
    std::lock_guard<std::mutex> guard(sGamepadMutex);
    sGamepadMenu = true;
    sGamepadPending = sGamepadProfile;
    sGamepadCloseAction = GamepadCloseAction::Save;
    if (!uif::gamepad::application_title(sGamepadTitle)) return;
    auto controls = WUPSConfigCategory::Create("GamePad buttons - this title");
    char title[32];
    std::snprintf(title, sizeof(title), "%016llX", static_cast<unsigned long long>(sGamepadTitle));
    controls.add(WUPSConfigItemStub::Create(title));
    controls.add(WUPSConfigItemBoolean::Create("gamepadEnabled", "Custom mapping", false,
                                             sGamepadProfile.enabled, gamepad_enabled_changed));
    const std::array<WUPSConfigItemMultipleValues::ValuePair, 3> actions = {{{0, "Save changes"},
        {1, "Discard changes"}, {2, "Restore standard controls"}}};
    controls.add(WUPSConfigItemMultipleValues::CreateFromValue("gamepadCloseAction", "On menu close", 0, 0,
                                                              actions, gamepad_close_action_changed));
    std::array<WUPSConfigItemMultipleValues::ValuePair, 16> choices;
    for (size_t i = 0; i < choices.size(); ++i) choices[i] = {static_cast<uint32_t>(i), kGamepadNames[i]};
    for (size_t i = 0; i < choices.size(); ++i) {
        char key[8];
        std::snprintf(key, sizeof(key), "gp.%02u", static_cast<unsigned>(i));
        const auto label = std::string(kGamepadNames[i]) + " ->";
        controls.add(WUPSConfigItemMultipleValues::CreateFromValue(key, label.c_str(), static_cast<int32_t>(i),
            sGamepadProfile.destinations[i], choices, gamepad_destination_changed));
    }
    if (!sGamepadError.empty()) controls.add(WUPSConfigItemStub::Create(sGamepadError));
    root.add(std::move(controls));
}

void gamepad_config_closed() {
    std::lock_guard<std::mutex> guard(sGamepadMutex);
    const auto was_open = sGamepadMenu;
    sGamepadMenu = false;
    if (!was_open || !uif::gamepad::application_title(sGamepadTitle)) return;
    // Samples consumed by Aroma's menu cannot leave a previously delivered key held in the title.
    sGamepadMapper.set(sGamepadProfile);
    const auto action = sGamepadCloseAction;
    sGamepadCloseAction = GamepadCloseAction::Save;
    if (action == GamepadCloseAction::Discard) {
        sGamepadPending = sGamepadProfile;
        return;
    }
    if (action == GamepadCloseAction::Restore) sGamepadPending = {};
    if (action != GamepadCloseAction::Restore && sGamepadPending.encode() == sGamepadProfile.encode()) return;
    if (!sGamepadPending.valid()) {
        sGamepadError = "Mapping not saved: every output button must appear once";
        return;
    }
    const auto key = gamepad_key(sGamepadTitle);
    std::string previous;
    const auto previousStatus = WUPSStorageAPI::Get(key, previous);
    if (previousStatus != WUPS_STORAGE_ERROR_SUCCESS && previousStatus != WUPS_STORAGE_ERROR_NOT_FOUND) {
        sGamepadError = "Mapping not applied: storage unavailable";
        return;
    }
    const auto encoded = sGamepadPending.encode();
    std::string staged;
    if (WUPSStorageAPI::Store(key, encoded) != WUPS_STORAGE_ERROR_SUCCESS
        || WUPSStorageAPI::Get(key, staged) != WUPS_STORAGE_ERROR_SUCCESS || staged != encoded
        || WUPSStorageAPI::SaveStorage() != WUPS_STORAGE_ERROR_SUCCESS) {
        // Restore the exact cached record, including absence/corruption, before other settings save.
        // WUPS does not promise a power-loss-safe disk transaction or rollback after an I/O error.
        const auto restored = previousStatus == WUPS_STORAGE_ERROR_SUCCESS
            ? WUPSStorageAPI::Store(key, previous) : WUPSStorageAPI::DeleteItem(key);
        bool recovered = restored == WUPS_STORAGE_ERROR_SUCCESS
            || (previousStatus == WUPS_STORAGE_ERROR_NOT_FOUND && restored == WUPS_STORAGE_ERROR_NOT_FOUND);
        if (recovered) {
            std::string readback;
            const auto status = WUPSStorageAPI::Get(key, readback);
            recovered = previousStatus == WUPS_STORAGE_ERROR_SUCCESS
                ? status == WUPS_STORAGE_ERROR_SUCCESS && readback == previous
                : status == WUPS_STORAGE_ERROR_NOT_FOUND;
        }
        sGamepadError = recovered ? "Mapping not applied: storage unavailable"
                                 : "Mapping not applied: storage recovery failed";
        return;
    }
    sGamepadProfile = sGamepadPending;
    sGamepadMapper.set(sGamepadProfile);
    sGamepadError.clear();
}

DECL_FUNCTION(int32_t, VPADRead, VPADChan channel, VPADStatus *samples, uint32_t capacity, VPADReadError *error) {
    const auto count = real_VPADRead(channel, samples, capacity, error);
    if (channel != VPAD_CHAN_0 || !samples || count <= 0 || static_cast<uint32_t>(count) > capacity
        || count > 16 || (error && *error != VPAD_READ_SUCCESS)) return count;
    WUPSConfigAPIMenuStatus menuStatus;
    if (WUPSConfigAPI_Menu_GetStatus(&menuStatus) != WUPSCONFIG_API_RESULT_SUCCESS
        || menuStatus != WUPSCONFIG_API_MENU_STATUS_CLOSED) return count;
    std::lock_guard<std::mutex> guard(sGamepadMutex);
    if (sGamepadMenu || !uif::gamepad::application_title(sGamepadTitle)) return count;
    // Remember delivered standard keys so enabling a profile can release them safely.
    for (auto i = count; i-- > 0;) {
        auto &sample = samples[i];
        if (sample.error != 0) continue;
        if (!sGamepadMapper.needs_processing()) {
            sGamepadMapper.observe_passthrough(sample.hold);
            continue;
        }
        const auto mapped = sGamepadMapper.apply({sample.hold, sample.trigger, sample.release});
        sample.hold = mapped.hold;
        sample.trigger = mapped.trigger;
        sample.release = mapped.release;
    }
    return count;
}
WUPS_MUST_REPLACE(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead);
