#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

constexpr int WUPS_STORAGE_ERROR_SUCCESS = 0, WUPSCONFIG_API_RESULT_SUCCESS = 0;
constexpr int WUPS_STORAGE_ERROR_NOT_FOUND = -0x10;
constexpr int VPAD_CHAN_0 = 0, VPAD_READ_SUCCESS = 0;
constexpr int WUPSCONFIG_API_MENU_STATUS_CLOSED = 0;
using VPADChan = int;
using VPADReadError = int;
using WUPSConfigAPIMenuStatus = int;
struct VPADStatus { uint32_t hold = 0, trigger = 0, release = 0; uint8_t error = 0; std::array<uint8_t, 128> untouched{}; };
static uint64_t testTitle = 0;
static int testReadCount = 1, testReadError = 0, testMenuStatus = 0, testMenuResult = 0;
static int testStoreError = 0, testSaveError = 0, testSaveCalls = 0;
static int testGetError = 0, testDeleteError = 0;
static int testStoreCalls = 0, testGetCalls = 0, testDeleteCalls = 0;
static int testFailStoreCall = 0, testFailGetCall = 0;
static bool testCorruptStore = false;
static VPADStatus testInput;
static std::map<std::string, std::string> testStorage;
static std::map<std::string, std::string> testDurableStorage;
inline uint64_t OSGetTitleID() { return testTitle; }
inline void OSReport(const char *, ...) {}
inline int WUPSConfigAPI_Menu_GetStatus(int *status) { *status = testMenuStatus; return testMenuResult; }
inline int32_t real_VPADRead(int, VPADStatus *samples, uint32_t capacity, int *error) {
    if (error) *error = testReadError;
    if (samples) for (uint32_t i = 0; i < capacity; ++i) samples[i] = testInput;
    return testReadCount;
}
namespace WUPSStorageAPI {
inline int Get(const std::string &key, std::string &value) {
    ++testGetCalls;
    if (testGetError || testGetCalls == testFailGetCall) return -6;
    auto it = testStorage.find(key);
    if (it == testStorage.end()) return WUPS_STORAGE_ERROR_NOT_FOUND;
    value = it->second; return 0;
}
inline int Store(const std::string &key, const std::string &value) {
    ++testStoreCalls;
    if (testStoreCalls == testFailStoreCall) return -6;
    if (testStoreError) return testStoreError;
    testStorage[key] = testCorruptStore ? "corrupt" : value; return 0;
}
inline int DeleteItem(const std::string &key) {
    ++testDeleteCalls;
    if (testDeleteError) return testDeleteError;
    return testStorage.erase(key) ? 0 : WUPS_STORAGE_ERROR_NOT_FOUND;
}
inline int SaveStorage() {
    ++testSaveCalls;
    if (!testSaveError) testDurableStorage = testStorage;
    return testSaveError;
}
}
struct ConfigItemBoolean {};
struct ConfigItemMultipleValues { const char *identifier; };
struct TestItem {
    std::string key, name;
    uint32_t value = 0;
    void (*booleanChanged)(ConfigItemBoolean *, bool) = nullptr;
    void (*multipleChanged)(ConfigItemMultipleValues *, uint32_t) = nullptr;
    std::vector<std::string> choices;
};
struct WUPSConfigItemStub {
    static TestItem Create(const std::string &label) { return {"", label, 0, nullptr, nullptr, {}}; }
};
struct WUPSConfigItemBoolean {
    static TestItem Create(const char *key, const char *name, bool, bool value, void (*callback)(ConfigItemBoolean *, bool)) {
        return {key, name, value, callback, nullptr, {}};
    }
};
struct WUPSConfigItemMultipleValues {
    struct ValuePair { uint32_t value; const char *name; };
    template<size_t N>
    static TestItem CreateFromValue(const char *key, const char *name, int32_t, uint32_t value,
        const std::array<ValuePair, N> &pairs, void (*callback)(ConfigItemMultipleValues *, uint32_t)) {
        TestItem item{key, name, value, nullptr, callback, {}};
        for (const auto &pair : pairs) item.choices.emplace_back(pair.name);
        return item;
    }
};
struct WUPSConfigCategory {
    std::string name;
    std::vector<TestItem> items;
    std::vector<WUPSConfigCategory> children;
    static WUPSConfigCategory Create(const char *name) { return {name, {}, {}}; }
    void add(TestItem item) { items.push_back(std::move(item)); }
    void add(WUPSConfigCategory child) { children.push_back(std::move(child)); }
};
#define DECL_FUNCTION(result, name, ...) result my_##name(__VA_ARGS__)
#define WUPS_MUST_REPLACE(...)
