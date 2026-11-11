# UInjectForge Aroma Bridge

Aroma Bridge is an optional Wii U plugin for discovery, SD setup status and per-title digital GamePad button remapping in native Wii U applications. Assistant is a separate application for transfers and installation. The Bridge does not transfer files, install titles or edit installed games.

## Install

Download a package from [Bridge releases](https://github.com/ZestyTS/UInjectForge-Aroma-Bridge/releases). Copy the contents of its `sd` folder to your Wii U SD card:

```text
sd:/wiiu/environments/aroma/plugins/uinjectforge-aroma-bridge.wps
```

Restart Aroma after replacing the plugin and check its active version/build ID in Diagnostics. Copying a file or reopening the menu does not prove that a replacement binary is running. Preserve existing plugin settings. Use a trusted local network; do not forward discovery or Assistant ports to the Internet.

## Discovery and setup status

The Bridge answers UIF discovery on UDP `8574`. UIF can fill the discovered Wii U address into its Assistant and FTP fields. Transfers still use Assistant on `8573`, an FTPiiU-compatible server or direct SD copy. Bridge discovery uses a different port so it can coexist with Assistant.

Open Aroma's plugin configuration and select **UInjectForge Bridge**:

- The main menu contains Discovery and connection status.
- Advanced network settings contains the UDP port and optional periodic beacons. Passive beacons default OFF.
- Diagnostics shows the version/build and cached SD setup status. An unfinished scan says Checking SD; unknown free space is not zero.

Setup checks look for Assistant, Bridge, FTPiiU, runtime files and installable-package folders. You do not need every listed runtime. Presence is not proof that a game or runtime works. Replies are bounded to 1,280 bytes. Detailed status and send diagnostics are written, when possible, to `sd:/uinjectforge/aroma-bridge/status.json`. Discovery does not upload this information to a developer service.

## GamePad buttons for the current title

In a native Wii U application, open **GamePad buttons - this title**. **Custom mapping defaults OFF.** Sixteen digital buttons can be reassigned. Every destination must appear exactly once, so swap both sides of a pair. Settings belong to the current full title ID and use the plugin's existing storage.

- **Save changes** applies a valid mapping after the menu closes and input returns to neutral. This action is selected each time the menu opens.
- **Discard changes** leaves stored and active controls unchanged.
- **Restore standard controls** disables mapping and restores all sixteen original destinations. Turning Custom mapping OFF alone retains destinations for later reuse.

HOME, TV, SYNC, analog axes, touch and motion remain unchanged. System menus do not apply the profile. This does not remap Wii/vWii, Wii Remote, Classic or Pro Controller input, or change general emulator settings. A native input hook is not a compatibility promise for every game or emulator.

Invalid or unsaved mappings retain the previous active profile. WUPS storage is shared with network settings; recovery after storage errors or power loss is not guaranteed. If a storage error appears, verify the saved settings before relying on persistence.

## Build and validation

See [BUILDING.md](BUILDING.md) for the pinned recipe and corresponding source, [VALIDATION.md](VALIDATION.md) for recorded observations and remaining checks, and [WUPS-SOURCE.md](WUPS-SOURCE.md) for relinking with modified WUPS. Original Bridge code is MIT licensed; WUPS and wut retain their separate terms in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) and `licenses/`.
