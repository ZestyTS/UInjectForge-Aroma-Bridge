# Building the Aroma Bridge

Run from this repository root with Docker installed:

```powershell
docker build -t uif-aroma-source .
$revision = git rev-parse --short=12 HEAD
docker run --rm --network none -v "${PWD}:/project" -w /project uif-aroma-source make -j2 UIF_COMPONENT_BUILD_ID=$revision
```

Output: `uinjectforge-aroma-bridge.wps`.

Package it at `sd/wiiu/environments/aroma/plugins/uinjectforge-aroma-bridge.wps`.
Include README.md, LICENSE, THIRD-PARTY-NOTICES.md and licenses/ in the ZIP,
plus checksums, source commit and the Docker image ID/digest.
Use a fresh checkout for release builds.

This source was exported from UIF commit
`629c424039246f5346259a4b448bb10236883c3a`,
directory `tools/UInjectForge.AromaBridge.WiiU`, without changing runtime code.
Older binaries identify their original UIF build rather than this repository.
Keep releases marked preview until discovery, settings, and coexistence with
Assistant/FTPiiU have been checked on hardware.
Changes here must be explicitly synchronized with the vendored copy in UIF.
