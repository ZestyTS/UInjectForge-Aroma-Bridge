# Validation scope for 1.0.6

## Current Mac host result

On November 10, the existing `sh tests/run-host.sh` suite passed all eight
C++ compile/run harnesses with Apple clang 21.0.0, assertions enabled, `-Werror`,
AddressSanitizer and UndefinedBehaviorSanitizer. It completed in 6.071 seconds,
including the exhaustive 65,536 digital-mask loop and 22 production-menu
transaction cases. Source bookends matched and temporary output was cleaned.
See [the compact result](validation/host-macos-20261110.json). This is host
coverage; no native WPS build or new console launch was performed in this run.

## Retained source and console observations

The earlier `src-423c3f8fca82` source and current `src-c7fb79bdbb79` source have the same seven production-input bodies after CRLF-to-LF normalization. Only Makefile and main.cpp line endings differ; the other five inputs are byte-identical. The current host runner additionally uses isolated temporary output and supports Mac/Linux compiler flags. This does not imply identical compiled bytes.

Earlier host coverage passed profile permutations, exhaustive digital input masks, production hook/menu callbacks, 22 save/discard/restore transactions, cached-storage errors, discovery serialization and readiness. The final release build needs its own logs and identity; earlier results are not a new build result.

The owner reported A/B swap, working HOME, Save and persistence after relaunch, valid-draft Discard, and Restore with standard controls retained after relaunch in **Hello Kitty Kruisers**, on the earlier verified development candidate. Region/revision were not recorded. This is a narrow owner observation, not an automated trace or a claim about all titles/buttons.

Still unobserved on that console: invalid-draft discard, title isolation, all sixteen destinations, held-key neutral transitions and storage-error recovery. Do not manufacture a hardware PASS from host tests or induce destructive storage faults. The final release artifact needs its own launch/active-identity observation; unchanged source behavior does not turn old binary evidence into new binary evidence.

No Wii/vWii input, broad emulator support, analog/touch/motion remapping or storage/power-loss guarantee is claimed.
