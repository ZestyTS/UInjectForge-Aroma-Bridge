# Third-party notices

Original UInjectForge Aroma Bridge source is MIT licensed.

The plugin links WiiUPluginSystem (WUPS), licensed under LGPL-3.0.
Its headers also retain individual permissive notices. WUPS is a separate
library; the plugin's MIT license does not replace those terms.
See licenses/WUPS.txt and licenses/GPL-3.0.txt.

Upstream library source: https://github.com/wiiu-env/WiiUPluginSystem
The Dockerfile selects the WUPS SDK image tagged 20260418.
For a binary release, preserve the exact library source revision/build
instructions corresponding to that image alongside the plugin source, so
recipients can rebuild/relink with a modified library. Do not represent the
whole compiled WPS as exclusively MIT licensed.

The plugin also links devkitPro wut (zlib license):
https://github.com/devkitPro/wut
See licenses/wut.txt. Individual dependency notices remain applicable.
