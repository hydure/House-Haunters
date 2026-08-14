#!/usr/bin/env python3
"""
generate_embedded_resources.py
==============================

Walks the project ``resources/`` directory and emits platform-specific files
into the CMake build tree so the entire resource bundle can be linked into the
final executable (no on-disk resources/ folder required):

  1. ``embedded_resources_ids.h``  -- one ``#define`` per resource,
                                       used by both the .rc and the table.
  2. ``embedded_resources.rc``     -- Win32 resource script that the MSVC
                                       resource compiler (rc.exe) turns
                                       into ``RT_RCDATA`` blobs baked
                                       into the .exe's resource section.
  3. ``embedded_resources_table.cpp`` -- C++ array mapping the relative
                                          POSIX path (e.g.
                                          ``"fonts/Underdog-Regular.ttf"``)
                                          to its numeric ID.

On macOS and Linux it instead emits ``embedded_resources_portable.cpp``, a
standard C++14 translation unit containing the bytes and the same lookup API.
This is deliberately compiler/linker neutral so Mach-O and ELF builds share
the same runtime ResourceFS code as Windows.

At runtime ``hh::ResourceFS`` looks up the path in the table, calls
``FindResourceA`` + ``LoadResource`` + ``LockResource`` to obtain a
pointer into the .exe's mapped image (zero-copy), and hands those bytes
to ``sf::Texture::loadFromMemory`` / ``sf::Font::loadFromMemory`` /
``ma_resource_manager_register_encoded_data`` etc.

Usage (invoked by CMake):

    python generate_embedded_resources.py <resources_dir> <output_dir> [windows|portable]

Skipped files (artist sources / docs that have no business shipping):

  * ``*.psd``
  * ``*.png.orig``
  * ``README.md`` (any case)
  * ``keep.txt`` (empty placeholders)
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

# Files that should never be embedded. Match case-insensitively against
# the bare filename (NOT against the relative path) so a future addition
# of e.g. resources/sprites/README.md is also caught automatically.
SKIP_FILENAMES = {"readme.md", "keep.txt"}
SKIP_SUFFIXES  = {".psd", ".orig"}


def should_skip(rel_path: Path) -> bool:
    name = rel_path.name.lower()
    if name in SKIP_FILENAMES:
        return True
    # ``Path.suffix`` only returns the final suffix; ``.png.orig`` etc.
    # need their last segment matched explicitly.
    if rel_path.suffix.lower() in SKIP_SUFFIXES:
        return True
    return False


def validate_path(rel_path: Path) -> None:
    """Hard-fail on any filename that cannot be represented in the
    generated .rc / .cpp / ninja files. Previously this function quietly
    skipped problematic names, which meant the offending asset never
    made it into the bundle and the bug only surfaced at runtime as a
    mysterious missing-resource. Failing loudly here forces the
    contributor to rename the file (the right answer in every case
    encountered so far -- artists pasting HTML-encoded apostrophes into
    filenames, copied-from-the-web assets with stray punctuation, etc.).

    The single character we genuinely can't accept is the double quote:
    rc.exe wraps every resource path in ``"..."`` and offers no escape
    sequence for an embedded quote. Every other "weird" character
    (apostrophe, ampersand, hash, semicolon) works once you drop the
    silent-skip behavior -- they don't break rc.exe's quoted-string
    parser, and CMake escapes them correctly when emitting ninja
    dependency lines.
    """
    posix = rel_path.as_posix()
    if '"' in posix:
        raise RuntimeError(
            "Resource path contains a double quote, which rc.exe cannot "
            f"embed (no escape sequence exists): {posix!r}. Rename the file."
        )


def gather_resources(resources_dir: Path) -> list[Path]:
    """Return sorted list of relative POSIX paths (under ``resources/``)
    that should be embedded. Sorting keeps the generated output stable so
    incremental builds don't re-link unnecessarily."""
    out: list[Path] = []
    for path in resources_dir.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(resources_dir)
        if should_skip(rel):
            continue
        # Hard-fail on names we genuinely can't embed. The check is here
        # rather than in should_skip so the failure mode is "explicit
        # error" not "silently missing at runtime".
        validate_path(rel)
        out.append(rel)
    # Sort by POSIX-style string so the order matches across OSes.
    out.sort(key=lambda p: p.as_posix())
    return out


def macro_name(index: int) -> str:
    """Resource IDs start at 1001 to stay clear of common Win32 reserved
    ranges (0..100 are typically icons/cursors in third-party templates)."""
    return f"HH_IDR_{index:04d}"


def write_ids_header(out_path: Path, files: list[Path]) -> None:
    lines = [
        "// GENERATED FILE -- do not edit. See tools/generate_embedded_resources.py",
        "#ifndef HH_EMBEDDED_RESOURCES_IDS_H",
        "#define HH_EMBEDDED_RESOURCES_IDS_H",
        "",
    ]
    for i, _rel in enumerate(files, start=1):
        lines.append(f"#define {macro_name(i)} {1000 + i}")
    lines.append("")
    lines.append("#endif  // HH_EMBEDDED_RESOURCES_IDS_H")
    lines.append("")
    out_path.write_text("\n".join(lines), encoding="ascii")


def write_rc_file(out_path: Path,
                  files: list[Path],
                  resources_dir: Path) -> None:
    """Emit a Win32 .rc script. We use absolute paths so the location of
    the .rc file in the build tree doesn't matter to rc.exe's search."""
    abs_resources = resources_dir.resolve()
    lines = [
        "// GENERATED FILE -- do not edit. See tools/generate_embedded_resources.py",
        '#include "embedded_resources_ids.h"',
        "",
    ]
    for i, rel in enumerate(files, start=1):
        abs_path = (abs_resources / rel).resolve()
        # rc.exe accepts forward slashes in paths but the convention is
        # double-backslash on Windows. Use forward slashes to avoid any
        # escape-sequence weirdness if a path happens to contain
        # ``\n``-looking sequences.
        rc_path = abs_path.as_posix()
        # validate_path() already guaranteed no embedded ``"``; rc.exe
        # has no escape sequence for quotes inside a path literal.
        lines.append(f'{macro_name(i)} RCDATA "{rc_path}"')
    lines.append("")
    out_path.write_text("\n".join(lines), encoding="ascii")


def cpp_string_literal(s: str) -> str:
    """Encode an arbitrary string as a C++ narrow string literal. We
    avoid raw-string literals because some POSIX paths may contain
    characters that confuse a fixed delimiter."""
    escaped = []
    for ch in s:
        if ch == "\\":
            escaped.append("\\\\")
        elif ch == '"':
            escaped.append('\\"')
        elif 32 <= ord(ch) < 127:
            escaped.append(ch)
        else:
            # Non-ASCII filenames: emit as \xHH hex. Embedded resource
            # paths are derived from filenames on disk, which on this
            # project are all ASCII -- but stay safe.
            escaped.append(f"\\x{ord(ch):02x}")
    return '"' + "".join(escaped) + '"'


def write_table_cpp(out_path: Path, files: list[Path]) -> None:
    lines = [
        "// GENERATED FILE -- do not edit. See tools/generate_embedded_resources.py",
        "//",
        "// Compiled only when HH_EMBED_RESOURCES is ON. Provides the path",
        "// -> Win32 resource ID mapping consumed by hh::ResourceFS.",
        "",
        '#include "engine/EmbeddedResources.hpp"',
        '#include "embedded_resources_ids.h"',
        "",
        "namespace hh { namespace embedded {",
        "",
        "namespace {",
        "constexpr Entry kTable[] = {",
    ]
    for i, rel in enumerate(files, start=1):
        # Store as forward-slash POSIX-style paths so lookups work the
        # same regardless of the call site's path separator habits.
        posix = rel.as_posix()
        lines.append(f"    {{ {cpp_string_literal(posix)}, {macro_name(i)} }},")
    lines.append("};")
    lines.append("constexpr std::size_t kCount = sizeof(kTable) / sizeof(kTable[0]);")
    lines.append("}  // namespace")
    lines.append("")
    lines.append("const Entry* table() noexcept { return kTable; }")
    lines.append("std::size_t  table_size() noexcept { return kCount; }")
    lines.append("")
    lines.append("}}  // namespace hh::embedded")
    lines.append("")
    out_path.write_text("\n".join(lines), encoding="ascii")


def write_portable_cpp(out_path: Path,
                       files: list[Path],
                       resources_dir: Path) -> None:
    """Emit a self-contained C++14 resource backend for Mach-O and ELF.

    Native linker-specific approaches would require separate implementations
    for each object format. Plain C++ is larger while compiling, but produces
    compact binary data and works with AppleClang, Clang, and GCC.
    """
    lines = [
        "// GENERATED FILE -- do not edit. See tools/generate_embedded_resources.py",
        "// Portable embedded-resource backend for macOS and Linux.",
        "",
        '#include "engine/EmbeddedResources.hpp"',
        "",
        "#include <cstring>",
        "",
        "namespace hh { namespace embedded {",
        "",
        "namespace {",
    ]

    sizes: list[int] = []
    for i, rel in enumerate(files):
        payload = (resources_dir / rel).read_bytes()
        sizes.append(len(payload))
        # Standard C++ forbids a zero-length array. Keep a one-byte sentinel
        # while recording the real size as zero in kPayloads below.
        encoded = payload if payload else b"\x00"
        lines.append(f"alignas(16) const unsigned char kData{i}[] = {{")
        for start in range(0, len(encoded), 16):
            chunk = encoded[start:start + 16]
            lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
        lines.append("};")
        lines.append("")

    lines.extend([
        "struct Payload {",
        "    const unsigned char* data;",
        "    std::size_t size;",
        "};",
        "",
        "constexpr Entry kTable[] = {",
    ])
    for rel in files:
        lines.append(f"    {{ {cpp_string_literal(rel.as_posix())}, 0 }},")
    lines.extend([
        "};",
        "",
        "const Payload kPayloads[] = {",
    ])
    for i, size in enumerate(sizes):
        lines.append(f"    {{ kData{i}, {size}u }},")
    lines.extend([
        "};",
        "",
        "constexpr std::size_t kCount = sizeof(kTable) / sizeof(kTable[0]);",
        "}  // namespace",
        "",
        "const Entry* table() noexcept { return kTable; }",
        "std::size_t table_size() noexcept { return kCount; }",
        "",
        "bool find(const char* posix_relative_path,",
        "          const void** data,",
        "          std::size_t* size) noexcept",
        "{",
        "    if (!posix_relative_path || !data || !size) return false;",
        "    for (std::size_t i = 0; i < kCount; ++i) {",
        "        if (std::strcmp(kTable[i].path, posix_relative_path) == 0) {",
        "            *data = kPayloads[i].data;",
        "            *size = kPayloads[i].size;",
        "            return true;",
        "        }",
        "    }",
        "    return false;",
        "}",
        "",
        "}}  // namespace hh::embedded",
        "",
    ])
    out_path.write_text("\n".join(lines), encoding="ascii")


def main(argv: list[str]) -> int:
    if len(argv) not in (3, 4):
        sys.stderr.write(
            "usage: generate_embedded_resources.py <resources_dir> <output_dir> "
            "[windows|portable]\n")
        return 2

    resources_dir = Path(argv[1])
    output_dir    = Path(argv[2])
    mode = argv[3] if len(argv) == 4 else "windows"

    if mode not in ("windows", "portable"):
        sys.stderr.write(f"error: unknown output mode: {mode}\n")
        return 2

    if not resources_dir.is_dir():
        sys.stderr.write(f"error: resources dir not found: {resources_dir}\n")
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)

    files = gather_resources(resources_dir)
    if not files:
        sys.stderr.write(f"error: no resources found under {resources_dir}\n")
        return 1

    if mode == "windows":
        ids_h = output_dir / "embedded_resources_ids.h"
        rc    = output_dir / "embedded_resources.rc"
        table = output_dir / "embedded_resources_table.cpp"

        write_ids_header(ids_h, files)
        write_rc_file(rc, files, resources_dir)
        write_table_cpp(table, files)
    else:
        portable = output_dir / "embedded_resources_portable.cpp"
        write_portable_cpp(portable, files, resources_dir)

    print(f"embedded {len(files)} resource(s) ({mode}) -> {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
