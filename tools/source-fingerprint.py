#!/usr/bin/env python3
"""Print the native-input fingerprint used by UIF's companion builder."""
import hashlib
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
paths = [root / "Makefile", root / "Dockerfile"]
for directory in ("source", "include", "assets", "app_booter_clean"):
    folder = root / directory
    if folder.exists():
        paths.extend(path for path in folder.rglob("*") if path.is_file())
paths.extend(path for path in root.iterdir() if path.is_file() and path.suffix in
             (".c", ".cc", ".cpp", ".h", ".hpp", ".s", ".S", ".ld", ".lds", ".py"))
if not 1 <= len(paths) <= 50000 or any(path.is_symlink() for path in paths):
    raise SystemExit("Invalid native source inventory")
rows = [f"{path.relative_to(root).as_posix()}\t{hashlib.sha256(path.read_bytes()).hexdigest()}" for path in paths]
# These native input paths are ASCII; case-insensitive order matches the original builder.
fingerprint = hashlib.sha256("\n".join(sorted(rows, key=str.casefold)).encode()).hexdigest()
if sys.argv[1:] not in ([], ["--build-id"]):
    raise SystemExit("Usage: source-fingerprint.py [--build-id]")
print("src-" + fingerprint[:12] if sys.argv[1:] else fingerprint)
