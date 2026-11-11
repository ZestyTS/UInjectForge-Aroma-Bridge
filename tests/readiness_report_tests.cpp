#include <cassert>
#include <cstdio>
#include <string>
#include "readiness_report.inc"

int main() {
    BridgeReadiness readiness{};
    auto summary = readiness_summary(readiness);
    assert(summary == "Checking SD...");
    auto json = readiness_json(readiness, summary);
    assert(json == "{\"schemaVersion\":1,\"summary\":\"Checking SD...\"}");
    readiness.checked = true;
    summary = readiness_summary(readiness);
    assert(summary == "SD not accessible");
    json = readiness_json(readiness, summary);
    assert(json.find("\"sdAccessible\":false") != std::string::npos);
    readiness.sdAccessible = true;
    readiness.assistantInstalled = true;
    readiness.validInstallPackages = 2;
    summary = readiness_summary(readiness);
    json = readiness_json(readiness, summary);
    assert(json.find("\"sdAccessible\":true") != std::string::npos);
    assert(json.find("\"assistantInstalled\":true") != std::string::npos);
    assert(json.find("\"validInstallPackages\":2") != std::string::npos);
    std::puts("Bridge readiness reports: pending, inaccessible and ready passed.");
}
