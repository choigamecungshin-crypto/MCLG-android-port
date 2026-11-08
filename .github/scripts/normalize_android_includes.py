#!/usr/bin/env python3
"""Normalize Windows-style quoted #include paths for Android/Clang builds.

The shared Legacy Edition sources were authored primarily for Windows and use
backslashes in relative include operands. Clang on Linux treats those as
literal filename characters, so those includes fail on GitHub's Android
runners. This script rewrites only include operands in the CI checkout; it
does not change string literals or other source text.
"""
from pathlib import Path
import re

ROOTS = (Path("Minecraft.World"), Path("Minecraft.Client"))
SOURCE_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".c", ".cc", ".cpp", ".cxx", ".inc"}
INCLUDE = re.compile(rb'^(\\s*#\\s*include\\s*)(["<])([^">\\r\\n]*\\\\[^">\\r\\n]*)([">])', re.MULTILINE)

def main() -> int:
    changed_files = 0
    changed_includes = 0

    for root in ROOTS:
        if not root.is_dir():
            raise SystemExit(f"Missing source root: {root}")

        for path in root.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
                continue

            original = path.read_bytes()
            def normalize(match: re.Match[bytes]) -> bytes:
                nonlocal changed_includes
                operand = match.group(3)
                normalized = operand.replace(b"\\\\", b"/")
                if normalized != operand:
                    changed_includes += 1
                return match.group(1) + match.group(2) + normalized + match.group(4)

            updated = INCLUDE.sub(normalize, original)
            if updated != original:
                path.write_bytes(updated)
                changed_files += 1

    print(f"Normalized {changed_includes} include paths across {changed_files} source files.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
