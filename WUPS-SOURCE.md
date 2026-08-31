# WUPS source and relinking

The pinned SDK image identifies this library source through its OCI metadata:

- Repository: https://github.com/wiiu-env/WiiUPluginSystem
- Revision: 6f1550fb41a3a8c53643a40cffd5405c59538d93
- Image: ghcr.io/wiiu-env/wiiupluginsystem:20260418
- Digest: sha256:91fe2c0b1fce10cb3744c670e57b161d4dd97f8c72cae7c5f9fb0ee2c69811ad
- Source: https://github.com/wiiu-env/WiiUPluginSystem/tree/6f1550fb41a3a8c53643a40cffd5405c59538d93

The included LGPL license was retrieved from that exact revision.
The original plugin source remains MIT; WUPS retains its own terms.

To rebuild with your own modified WUPS library:

1. Clone the WUPS repository and check out the revision above.
2. Make your library changes and run `docker build -t my-wups-sdk .`
   from that WUPS checkout (its Dockerfile exports /artifacts).
3. From this plugin checkout, run
   `docker build --build-arg WUPS_IMAGE=my-wups-sdk -t my-bridge-builder .`.
4. Compile a fresh plugin checkout using the Docker command in BUILDING.md,
   substituting `my-bridge-builder` for `uif-aroma-source`.

For published binary releases, provide this plugin's corresponding source,
this exact library source or an accompanying source download, build
instructions, and both license texts. Do not impose terms that prevent
relinking or debugging modifications to the library.
