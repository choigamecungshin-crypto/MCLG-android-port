#!/usr/bin/env python3
"""Normalize Windows-style include separators for Android/Clang builds.

The Legacy Edition shared sources use Windows backslashes in many relative
#include operands. Clang on Linux treats those as literal filename characters.
This script rewrites only include operands in the CI checkout; source strings,
comments, and non-include lines are left untouched.
"""
from pathlib import Path

ROOTS = (Path("Minecraft.World"), Path("Minecraft.Client"))
SOURCE_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".c", ".cc", ".cpp", ".cxx", ".inc"}
BACKSLASH = bytes([92])

def normalize_line(line: bytes) -> tuple[bytes, bool]:
    hash_pos = line.find(b"#")
    if hash_pos < 0 or line[:hash_pos].strip():
        return line, False

    directive = line[hash_pos + 1:].lstrip()
    if not directive.startswith(b"include"):
        return line, False

    quote_pos = line.find(b'"', hash_pos + 1)
    angle_pos = line.find(b"<", hash_pos + 1)
    candidates = [p for p in (quote_pos, angle_pos) if p >= 0]
    if not candidates:
        return line, False

    open_pos = min(candidates)
    close_char = b'"' if line[open_pos:open_pos + 1] == b'"' else b">"
    close_pos = line.find(close_char, open_pos + 1)
    if close_pos < 0:
        return line, False

    operand = line[open_pos + 1:close_pos]
    if BACKSLASH not in operand:
        return line, False

    normalized = operand.replace(BACKSLASH, b"/")
    return (
        line[:open_pos + 1] + normalized + line[close_pos:],
        True,
    )

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
            output = []
            file_changed = False
            for line in original.splitlines(keepends=True):
                updated, changed = normalize_line(line)
                output.append(updated)
                if changed:
                    changed_includes += 1
                    file_changed = True

            if file_changed:
                path.write_bytes(b"".join(output))
                changed_files += 1

    print(f"Normalized {changed_includes} include paths across {changed_files} source files.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
