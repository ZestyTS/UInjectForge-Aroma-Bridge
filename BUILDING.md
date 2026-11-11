# Building Aroma Bridge 1.0.6

Use the reviewed clean standalone release commit. `SOURCE-MAPPING.json` maps native inputs, host tests and licenses to their originating committed UIF source. Public documentation/workflow changes do not alter the mapped production bodies. The Dockerfile pins both SDK image digests.

With Docker, Git, Python 3 and a host C++ compiler installed, run from the repository root:

```sh
sh tests/run-host.sh
fingerprint=$(python3 tools/source-fingerprint.py)
build_id=$(python3 tools/source-fingerprint.py --build-id)
commit=$(git rev-parse HEAD)
test -z "$(git status --porcelain --untracked-files=no)"
docker build -t uif-aroma-source .
image=$(docker image inspect uif-aroma-source --format '{{.Id}}')
docker run --rm --network none -v "$PWD:/project" -w /project "$image" make clean
docker run --rm --network none -v "$PWD:/project" -w /project "$image" make -j2 UIF_COMPONENT_VERSION=1.0.6 UIF_COMPONENT_BUILD_ID="$build_id"
test "$fingerprint" = "$(python3 tools/source-fingerprint.py)"
git archive --format=zip --prefix=UInjectForge-Aroma-Bridge-1.0.6-source/ --output=UInjectForge-Aroma-Bridge-1.0.6-source.zip "$commit"
```

Output: `uinjectforge-aroma-bridge.wps`. Keep the build log, source commit, native fingerprint, component version/build ID, immutable builder image ID, output length and SHA-256 in the release's `BUILD-INFO.json`. A successful compile is not a Wii U launch result. Do not substitute the historical development binary.

## Package recipe

Use a fresh staging directory for `UInjectForge-Aroma-Bridge-1.0.6.zip`:

```text
sd/wiiu/environments/aroma/plugins/uinjectforge-aroma-bridge.wps
README.md
BUILDING.md
VALIDATION.md
SOURCE-MAPPING.json
BUILD-INFO.json
LICENSE
THIRD-PARTY-NOTICES.md
WUPS-SOURCE.md
licenses/GPL-3.0.txt
licenses/WUPS.txt
licenses/wut.txt
CHECKSUMS-SHA256.txt
```

Generate hashes of the staged files excluding the checksum file itself. Independently reopen the ZIP and compare every member with staging. Do not ship plugin storage, SD status, user configuration, private logs or local development paths.

Provide separate release assets:

- `UInjectForge-Aroma-Bridge-1.0.6-source.zip`, from that same clean Git commit.
- Exact WUPS source at `6f1550fb41a3a8c53643a40cffd5405c59538d93`, downloaded from upstream with its license/build instructions; see [WUPS-SOURCE.md](WUPS-SOURCE.md).

Hash all release assets. The desktop updater must admit the final package's actual WPS hash; a package's own manifest is not a trust anchor. Publish after final source/package review and the applicable validation record are complete. Plain make without explicit component version/build ID still produces development metadata.
