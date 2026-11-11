#!/bin/sh
set -eu
work=$(mktemp -d "${TMPDIR:-/tmp}/uif-aroma-host.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
awk '/^[[:space:]]*WUPS_USE_WUT_DEVOPTAB\(\);[[:space:]]*$/ { count++ } END { if (count != 1) { print "Bridge must register WUT filesystem initialization exactly once"; exit 1 } }' source/main.cpp
# Linux WSL needs fixed placement to avoid sanitizer startup faults; Darwin does not support these linker flags.
non_pie=
case "$(uname -s)" in Linux) non_pie="-fno-pie -no-pie" ;; esac
c++ -std=c++17 -Wall -Wextra -Werror -pthread $non_pie -UNDEBUG -fsanitize=address,undefined -Iinclude tests/readiness_cache_tests.cpp -o "$work/uif-readiness-tests"
"$work/uif-readiness-tests"
c++ -std=c++17 -Wall -Wextra -Werror $non_pie -UNDEBUG -fsanitize=address,undefined -Iinclude tests/gamepad_profile_tests.cpp -o "$work/uif-gamepad-profile-tests"
"$work/uif-gamepad-profile-tests"
c++ -std=c++17 -Wall -Wextra -Werror -pthread $non_pie -UNDEBUG -fsanitize=address,undefined -Iinclude -Itests tests/gamepad_handler_tests.cpp -o "$work/uif-gamepad-handler-tests"
"$work/uif-gamepad-handler-tests"
c++ -std=c++17 -Wall -Wextra -Werror -pthread $non_pie -UNDEBUG -fsanitize=address,undefined -Iinclude -Itests tests/gamepad_menu_transaction_tests.cpp -o "$work/uif-gamepad-menu-transaction-tests"
"$work/uif-gamepad-menu-transaction-tests"
awk '/^struct BridgeReadiness / { copy=1 } copy { print } copy && /^};/ { copy=0 }' source/main.cpp > "$work/readiness_report.inc"
awk '/^static std::string readiness_summary\(/ || /^static std::string readiness_json\(/ { copy=1 } copy { print } copy && /^}/ { copy=0 }' source/main.cpp >> "$work/readiness_report.inc"
c++ -std=c++17 -Wall -Wextra -Werror $non_pie -UNDEBUG -fsanitize=address,undefined -I"$work" tests/readiness_report_tests.cpp -o "$work/report-tests"
"$work/report-tests"
awk '/^struct BridgeReadiness / { copy=1 } copy { print } copy && /^};/ { copy=0 }' source/main.cpp > "$work/readiness_menu.inc"
awk '/^static std::string readiness_summary\(/ { copy=1 } copy { print } copy && /^}/ { copy=0 }' source/main.cpp >> "$work/readiness_menu.inc"
printf '\nstatic BridgeReadiness readiness_snapshot(bool) { return {}; }\n' >> "$work/readiness_menu.inc"
awk '/^static WUPSConfigAPICallbackStatus config_opened\(/ { copy=1 } copy { print } copy && /^}/ { copy=0 }' source/main.cpp >> "$work/readiness_menu.inc"
c++ -std=c++17 -Wall -Wextra -Werror $non_pie -UNDEBUG -fsanitize=address,undefined -I"$work" tests/config_menu_tests.cpp -o "$work/menu-tests"
"$work/menu-tests"
awk '/^static void send_discovery_datagram\(/ { copy=1 } copy { print } copy && /^}/ { copy=0 }' source/main.cpp > "$work/discovery_send.inc"
c++ -std=c++17 -Wall -Wextra -Werror $non_pie -UNDEBUG -fsanitize=address,undefined -I"$work" tests/discovery_send_tests.cpp -o "$work/send-tests"
"$work/send-tests"
awk '/^struct BridgeReadiness / { copy=1 } copy { print } copy && /^};/ { copy=0 }' source/main.cpp > "$work/discovery_payload.inc"
awk '/^static std::string readiness_json\(/ || /^static std::string build_beacon_json\(/ { copy=1 } copy { print } copy && /^}/ { copy=0 }' source/main.cpp >> "$work/discovery_payload.inc"
c++ -std=c++17 -Wall -Wextra -Werror $non_pie -UNDEBUG -fsanitize=address,undefined -I"$work" tests/discovery_payload_tests.cpp -o "$work/payload-tests"
"$work/payload-tests"
