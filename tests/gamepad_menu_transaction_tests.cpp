#define UIF_GAMEPAD_HOST_TEST
#include "../source/gamepad_controls.cpp"
#include <algorithm>
#include <cassert>
#include <optional>

using namespace uif::gamepad;
static constexpr uint64_t title = 0x0005000012345678ULL;
static unsigned cases = 0;

static Profile swapped() {
    Profile profile;
    profile.enabled = true;
    std::swap(profile.destinations[0], profile.destinations[1]);
    return profile;
}

static void reset(std::optional<std::string> record = std::nullopt) {
    gamepad_application_end();
    testTitle = title;
    testReadCount = 1;
    testReadError = testMenuStatus = testMenuResult = 0;
    testStoreError = testSaveError = testGetError = testDeleteError = 0;
    testStoreCalls = testGetCalls = testDeleteCalls = testSaveCalls = 0;
    testFailStoreCall = testFailGetCall = 0;
    testCorruptStore = false;
    testInput = {};
    testStorage = {{"bridgePort", "8574"}, {gamepad_key(title + 1), swapped().encode()}};
    if (record) testStorage[gamepad_key(title)] = *record;
    testDurableStorage = testStorage;
    gamepad_application_start();
}

static WUPSConfigCategory open() {
    auto menu = WUPSConfigCategory::Create("root");
    gamepad_config_opened(menu);
    return menu;
}

static const TestItem &item(const WUPSConfigCategory &menu, const std::string &key) {
    const auto &items = menu.children.at(0).items;
    const auto found = std::find_if(items.begin(), items.end(), [&](const auto &entry) { return entry.key == key; });
    assert(found != items.end());
    return *found;
}

static void change(const WUPSConfigCategory &menu, const std::string &key, uint32_t value) {
    const auto &entry = item(menu, key);
    if (entry.booleanChanged) entry.booleanChanged(nullptr, value != 0);
    else {
        assert(entry.multipleChanged);
        ConfigItemMultipleValues sdkItem{entry.key.c_str()};
        entry.multipleChanged(&sdkItem, value);
    }
}

static VPADStatus read() {
    VPADStatus sample;
    assert(my_VPADRead(0, &sample, 1, nullptr) == 1);
    return sample;
}

int main() {
    reset();
    auto menu = open();
    const auto &action = item(menu, "gamepadCloseAction");
    assert(action.value == 0 && action.name == "On menu close");
    assert((action.choices == std::vector<std::string>{"Save changes", "Discard changes", "Restore standard controls"}));
    assert(item(menu, "gp.00").name == "A ->" && item(menu, "gp.15").name == "Right stick ->");
    assert(item(menu, "gamepadEnabled").value == 0);
    ++cases;
    gamepad_config_closed();
    assert(testStoreCalls == 0 && testSaveCalls == 0);
    ++cases;

    for (bool actionFirst : {false, true}) {
        reset(swapped().encode());
        const auto before = testStorage;
        menu = open();
        if (actionFirst) change(menu, "gamepadCloseAction", 1);
        change(menu, "gp.00", 0); // Invalid draft must also be discardable.
        change(menu, "gamepadEnabled", 0);
        if (!actionFirst) change(menu, "gamepadCloseAction", 1);
        gamepad_config_closed();
        assert(testStorage == before && testDurableStorage == before);
        assert(testStoreCalls == 0 && testSaveCalls == 0 && sGamepadProfile.encode() == swapped().encode());
        menu = open();
        assert(item(menu, "gamepadCloseAction").value == 0 && item(menu, "gp.00").value == 1);
        gamepad_config_closed();
        ++cases;
    }

    for (bool actionFirst : {false, true}) {
        reset(swapped().encode());
        read(); // Arm the active mapping with neutral input.
        testInput.hold = testInput.trigger = buttons[0];
        testInput.untouched.fill(0xAB);
        assert(read().hold == buttons[1]);
        menu = open();
        if (actionFirst) change(menu, "gamepadCloseAction", 2);
        change(menu, "gp.00", 0);
        change(menu, "gamepadEnabled", 1);
        if (!actionFirst) change(menu, "gamepadCloseAction", 2);
        gamepad_config_closed();
        assert(sGamepadProfile.encode() == Profile{}.encode());
        assert(testStorage.at(gamepad_key(title)) == Profile{}.encode() && testDurableStorage == testStorage);
        assert(testStorage.at(gamepad_key(title + 1)) == swapped().encode() && testStorage.at("bridgePort") == "8574");
        auto sample = read();
        assert(sample.hold == 0 && sample.release == buttons[1] && sample.untouched == testInput.untouched);
        assert(read().release == 0);
        testInput.hold = testInput.trigger = 0;
        read();
        testInput.hold = testInput.trigger = buttons[0];
        assert(read().hold == buttons[0]);
        testStorage = testDurableStorage;
        gamepad_application_start();
        menu = open();
        assert(item(menu, "gamepadCloseAction").value == 0 && item(menu, "gamepadEnabled").value == 0);
        for (unsigned i = 0; i < 16; ++i) assert(sGamepadPending.destinations[i] == i);
        gamepad_config_closed();
        assert(testSaveCalls == 1);
        ++cases;
    }

    for (const auto &record : {std::string("1E0000000000000000"), std::string("")}) {
        reset(record);
        assert(!sGamepadError.empty() && !sGamepadProfile.enabled);
        menu = open();
        change(menu, "gamepadCloseAction", 2);
        gamepad_config_closed();
        assert(testStorage.at(gamepad_key(title)) == Profile{}.encode() && sGamepadError.empty());
        assert(testSaveCalls == 1 && testDurableStorage == testStorage);
        ++cases;
    }

    for (const auto &record : {std::optional<std::string>{}, std::optional<std::string>{swapped().encode()},
                              std::optional<std::string>{"invalid"}}) {
        reset(record);
        const auto before = testStorage;
        menu = open();
        change(menu, "gamepadCloseAction", 2);
        testSaveError = -6;
        gamepad_config_closed();
        assert(testStorage == before && testDurableStorage == before);
        assert(sGamepadProfile.encode() == (record == swapped().encode() ? swapped().encode() : Profile{}.encode()));
        assert(sGamepadError == "Mapping not applied: storage unavailable");
        assert(testDeleteCalls == (record ? 0 : 1));
        testSaveError = 0;
        assert(WUPSStorageAPI::SaveStorage() == 0 && testDurableStorage == before); // Parent's later save.
        ++cases;
    }

    reset(swapped().encode());
    menu = open();
    change(menu, "gamepadCloseAction", 2);
    testGetError = -6;
    gamepad_config_closed();
    assert(testStoreCalls == 0 && testSaveCalls == 0 && sGamepadProfile.enabled);
    ++cases;

    reset(swapped().encode());
    const auto before = testStorage;
    menu = open();
    change(menu, "gamepadCloseAction", 2);
    testFailGetCall = testGetCalls + 2; // Snapshot succeeds, staged readback fails.
    gamepad_config_closed();
    assert(testStorage == before && testSaveCalls == 0 && sGamepadProfile.enabled);
    ++cases;

    reset(swapped().encode());
    menu = open();
    change(menu, "gamepadCloseAction", 2);
    testFailStoreCall = 1;
    gamepad_config_closed();
    assert(testStorage == before && testSaveCalls == 0 && sGamepadProfile.enabled);
    ++cases;

    reset();
    menu = open();
    change(menu, "gamepadEnabled", 1);
    testCorruptStore = true;
    gamepad_config_closed();
    assert(testStorage == testDurableStorage && testSaveCalls == 0 && !sGamepadProfile.enabled);
    ++cases;

    reset(swapped().encode());
    menu = open();
    change(menu, "gamepadCloseAction", 2);
    testCorruptStore = true;
    gamepad_config_closed();
    assert(testSaveCalls == 0 && sGamepadProfile.enabled);
    assert(sGamepadError == "Mapping not applied: storage recovery failed");
    ++cases;

    reset(swapped().encode());
    menu = open();
    change(menu, "gamepadCloseAction", 2);
    testSaveError = -6;
    testFailStoreCall = 2; // Recovery itself is an external failure, not an atomicity guarantee.
    gamepad_config_closed();
    assert(sGamepadProfile.enabled && testDurableStorage.at(gamepad_key(title)) == swapped().encode());
    assert(sGamepadError == "Mapping not applied: storage recovery failed");
    ++cases;

    reset();
    menu = open();
    change(menu, "gamepadEnabled", 1);
    testSaveError = testDeleteError = -6;
    gamepad_config_closed();
    assert(!sGamepadProfile.enabled && testDurableStorage.count(gamepad_key(title)) == 0);
    assert(sGamepadError == "Mapping not applied: storage recovery failed");
    ++cases;

    reset();
    testGetError = -6;
    gamepad_application_start();
    assert(!sGamepadProfile.enabled && sGamepadError == "Mapping unavailable; standard controls retained");
    ++cases;

    reset();
    menu = open();
    ConfigItemMultipleValues wrong{"gp.00"}, valid{"gamepadCloseAction"};
    gamepad_close_action_changed(nullptr, 2);
    gamepad_close_action_changed(&wrong, 2);
    gamepad_close_action_changed(&valid, 3);
    assert(sGamepadCloseAction == GamepadCloseAction::Save);
    gamepad_config_closed();
    gamepad_close_action_changed(&valid, 2);
    gamepad_config_closed();
    assert(testStoreCalls == 0 && testSaveCalls == 0);
    ++cases;

    reset(swapped().encode());
    menu = open();
    change(menu, "gamepadEnabled", 0);
    gamepad_config_closed();
    assert(!sGamepadProfile.enabled && sGamepadProfile.destinations == swapped().destinations);
    assert(testDurableStorage.at(gamepad_key(title)) == sGamepadProfile.encode());
    ++cases;

    reset(swapped().encode());
    menu = open();
    change(menu, "gamepadCloseAction", 2);
    gamepad_application_end();
    gamepad_config_closed();
    assert(testStoreCalls == 0 && testSaveCalls == 0);
    ++cases;

    std::printf("Production GamePad menu transactions: %u cases PASS (save/discard/reset, callback order, exact rollback, recovery failures, persistence).\n", cases);
}
