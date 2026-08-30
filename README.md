# UInjectForge Aroma Bridge

`UInjectForge Aroma Bridge` is a lightweight Aroma/WUPS plugin for Wii U.

The bridge does not transfer or install files. It gives UInjectForge a low-risk way to confirm that the Wii U is reachable while Aroma is running, and it reports a small read-only setup checklist so testers can see whether the obvious console-side pieces are present. Transfers still use one of these paths:

- UInjectForge Assistant WUHB
- FTPiiU / FTPiiU Everywhere compatible FTP transfer
- direct SD copy

## Install

Copy the packaged `sd` folder to the root of the Wii U SD card.

Expected path:

```text
sd:/wiiu/environments/aroma/plugins/uinjectforge-aroma-bridge.wps
```

Restart Aroma or reload plugins after copying.

## Discovery

The bridge listens for UIF discovery probes on UDP port `8574` by default. UIF Desktop scans the normal Assistant port first and then the bridge port, so the existing **Find Wii U** action can report whether the bridge is alive.

When UIF finds the bridge, it can fill the Wii U IP into the FTP and Assistant host fields. It still leaves actual transfers to the Assistant app on `8573`, FTPiiU-compatible FTP on `21`, or direct SD copy.

The bridge intentionally uses a different port from the Assistant app so it does not block the Assistant receiver from binding UDP `8573`.

Bridge discovery and passive beacons can include:

- component version, source build ID, and supported capability list
- SD accessibility and free space
- Assistant app presence under `sd:/wiiu/apps/uinjectforge-assistant/`
- Aroma Bridge plugin presence under `sd:/wiiu/environments/aroma/plugins/`
- FTPiiU-compatible plugin presence under `sd:/wiiu/environments/aroma/plugins/`
- Nintendont presence under `sd:/apps/nintendont/` or `sd:/apps/Nintendont/`
- NooDS and PPSSPP runtime/assets readiness
- WiiStation, Not64 (including adjacent `data.iso`), and Genesis Plus GX vWii preset readiness
- controller INI and Loadiine folder presence
- valid versus incomplete package-folder counts under `sd:/install/`, bounded to 512 folders

The beacon keeps the older flat readiness fields and also includes the shared versioned `setup` object used by current UIF Desktop/mobile clients.

The additive `setup` fields `wiiStationInstalled`, `not64Installed`, and `genesisPlusGxInstalled` let UIF validate guided vWii forwarder presets without requiring the SD card to be mounted on the client device. Missing fields from an older plugin are treated as unknown, not failed.

The bridge also writes a best-effort status file to:

```text
sd:/uinjectforge/aroma-bridge/status.json
```

## Aroma Settings

Open Aroma's plugin configuration and choose `UInjectForge Bridge`.

Options:

- `Enable bridge discovery`
- `Discovery UDP port`
- `Passive discovery beacon`

Passive beacons are off by default. Active discovery replies are enough for normal UIF Desktop/mobile discovery and are less noisy on tester networks.
