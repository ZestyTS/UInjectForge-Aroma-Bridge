#define UIF_GAMEPAD_HOST_TEST
#include "../source/gamepad_controls.cpp"
#include <cassert>

static VPADStatus read(int channel = 0, uint32_t capacity = 1, int *error = nullptr) {
    VPADStatus sample;
    assert(my_VPADRead(channel, &sample, capacity, error) == testReadCount);
    return sample;
}

int main() {
    using namespace uif::gamepad;
    testInput.hold = 0x8000 | 2;
    testInput.trigger = 0x8000;
    testInput.untouched.fill(0xAB);
    for (auto title : {0x0005001010040000ULL, 0x0005000E12345678ULL, 0ULL}) {
        testTitle = title;
        gamepad_application_start();
        assert(read().hold == testInput.hold);
        auto menu = WUPSConfigCategory::Create("root");
        gamepad_config_opened(menu);
        assert(menu.children.empty());
        gamepad_config_closed();
    }
    testTitle = 0x00050002F1234567ULL;
    gamepad_application_start();
    assert(read().hold == testInput.hold && testStorage.empty());
    auto menu = WUPSConfigCategory::Create("root");
    gamepad_config_opened(menu);
    assert(menu.children.size() == 1 && menu.children[0].items.size() == 19);
    assert(menu.children[0].items[1].value == 0);
    gamepad_enabled_changed(nullptr, true);
    ConfigItemMultipleValues a{"gp.00"}, b{"gp.01"};
    gamepad_destination_changed(&a, 1);
    gamepad_destination_changed(&b, 0);
    assert(read().hold == testInput.hold); // Menu bypass.
    gamepad_config_closed();
    assert(testSaveCalls == 1 && testStorage.size() == 1 && sGamepadProfile.enabled);
    assert(testStorage.begin()->first == "gamepad.00050002F1234567");
    auto mapped = read(); // Release the unmapped key already delivered before enabling.
    assert(mapped.hold == 2 && mapped.release == buttons[0] && mapped.untouched == testInput.untouched);
    assert(read().release == 0); // Release exactly once while waiting for neutral.
    testInput.hold = testInput.trigger = 0;
    read();
    testInput.hold = testInput.trigger = buttons[0];
    mapped = read();
    assert(mapped.hold == buttons[1] && mapped.trigger == buttons[1] && mapped.untouched == testInput.untouched);
    testInput.release = buttons[0] | 2 | 0x10000;
    mapped = read();
    assert(mapped.release == (buttons[1] | 2 | 0x10000));
    testInput.release = 0;
    gamepad_config_opened(menu);
    testInput.hold = testInput.trigger = 0; // Released while Aroma owns the input.
    gamepad_config_closed();
    mapped = read();
    assert(mapped.release == buttons[1] && mapped.hold == 0 && testSaveCalls == 1);
    assert(read().release == 0);
    testInput.hold = testInput.trigger = buttons[0];
    assert(read().hold == buttons[1]);
    for (auto menuStatus : {1, 2}) { testMenuStatus = menuStatus; assert(read().hold == buttons[0]); }
    testMenuStatus = 0; testMenuResult = 1;
    assert(read().hold == buttons[0]);
    testMenuResult = 0;
    assert(read(1).hold == buttons[0]);
    testReadError = 1; int error = 0;
    assert(read(0, 1, &error).hold == buttons[0]);
    testReadError = 0; testInput.error = 1;
    assert(read().hold == buttons[0]);
    testInput.error = 0;
    for (auto count : {-1, 0, 2, 17}) { testReadCount = count; assert(read().hold == buttons[0]); }
    testReadCount = 1;
    assert(my_VPADRead(0, nullptr, 0, nullptr) == 1);

    gamepad_config_opened(menu);
    gamepad_destination_changed(&a, 0); // Duplicate output -> reject entire transaction.
    const auto stored = testStorage;
    gamepad_config_closed();
    assert(testStorage == stored && sGamepadProfile.destinations[0] == 1 && !sGamepadError.empty());
    gamepad_config_opened(menu);
    gamepad_enabled_changed(nullptr, false);
    testSaveError = 1;
    gamepad_config_closed();
    assert(testStorage == stored && sGamepadProfile.enabled && !sGamepadError.empty());
    testSaveError = 0;
    gamepad_config_opened(menu);
    gamepad_enabled_changed(nullptr, false);
    gamepad_config_closed();
    mapped = read();
    assert(mapped.hold == 0 && (mapped.release & buttons[1]));
    testInput.hold = testInput.trigger = 0;
    read();
    testInput.hold = buttons[0];
    assert(read().hold == buttons[0]);
    gamepad_application_end();
    assert(read().hold == buttons[0]);
    testTitle = 0x00050002F7654321ULL;
    gamepad_application_start();
    assert(!sGamepadProfile.enabled && read().hold == buttons[0]);
    testStorage[gamepad_key(testTitle)] = "1E0000000000000000";
    gamepad_application_start();
    assert(!sGamepadProfile.enabled && read().hold == buttons[0] && !sGamepadError.empty());
    std::puts("Production GamePad hook/menu/storage: isolation, opt-in, all bypasses, failed saves, neutral enable/disable and non-button preservation PASS.");
}
