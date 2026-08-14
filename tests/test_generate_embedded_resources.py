"""Unit tests for tools/generate_embedded_resources.py.

Pin the contract that the generator (a) emits both Win32 and portable
resource backends cleanly, (b) skips known artist sources, and (c) hard-fails
on filenames that genuinely can't be embedded rather than silently dropping
them.

Runs via either pytest (``pytest tests/test_generate_embedded_resources.py``)
or directly (``python tests/test_generate_embedded_resources.py``).
"""
from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path

# Make tools/ importable so we exercise the real module.
_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(_REPO_ROOT, "tools"))

import generate_embedded_resources as gen  # noqa: E402


def _seed(tmp: Path, rel: str, body: bytes = b"x") -> None:
    """Create a file at ``tmp/rel`` with ``body``, making intermediate
    directories as needed."""
    p = tmp / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(body)


class GatherResourcesTests(unittest.TestCase):
    def test_walks_directory_and_returns_sorted_posix_keys(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            # Seed in a non-sorted order to verify the generator sorts.
            _seed(tmp, "zeta.bin")
            _seed(tmp, "alpha/beta.bin")
            _seed(tmp, "alpha/aaa.bin")
            files = gen.gather_resources(tmp)
            keys = [p.as_posix() for p in files]
            self.assertEqual(keys, ["alpha/aaa.bin", "alpha/beta.bin", "zeta.bin"])

    def test_skips_artist_sources_and_placeholders(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            _seed(tmp, "real.png")
            _seed(tmp, "art.psd")             # SKIP_SUFFIXES (.psd)
            _seed(tmp, "art.png.orig")        # SKIP_SUFFIXES (.orig)
            _seed(tmp, "README.md")           # SKIP_FILENAMES
            _seed(tmp, "sub/keep.txt")        # SKIP_FILENAMES
            files = [p.as_posix() for p in gen.gather_resources(tmp)]
            self.assertEqual(files, ["real.png"])

    def test_accepts_filenames_with_apostrophes_and_ampersands(self) -> None:
        # Regression: the old blocklist silently dropped these. The new
        # contract is "embed them; only refuse what genuinely can't be
        # represented in the .rc". rc.exe handles apostrophes, &, ;, #
        # inside quoted path literals just fine.
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            _seed(tmp, "art/akquot.png")          # baseline
            _seed(tmp, "art/aks_assets.png")      # baseline
            _seed(tmp, "art/joe's_stuff.png")     # apostrophe
            _seed(tmp, "art/a&b.png")             # ampersand
            _seed(tmp, "art/note;1.png")          # semicolon
            _seed(tmp, "art/note#1.png")          # hash
            files = sorted(p.as_posix() for p in gen.gather_resources(tmp))
            self.assertEqual(files, sorted([
                "art/akquot.png",
                "art/aks_assets.png",
                "art/joe's_stuff.png",
                "art/a&b.png",
                "art/note;1.png",
                "art/note#1.png",
            ]))

    def test_rejects_double_quote_in_filename(self) -> None:
        # rc.exe has no escape sequence for a ``"`` inside a quoted path
        # literal, so we hard-fail rather than emit ambiguous .rc text.
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            # NTFS forbids ``"`` in filenames, so simulate via the Path
            # API directly instead of a real on-disk file. validate_path
            # is the contract under test.
            with self.assertRaises(RuntimeError) as ctx:
                gen.validate_path(Path('art/quote".png'))
            self.assertIn("double quote", str(ctx.exception))


class WriteOutputsTests(unittest.TestCase):
    """End-to-end: run the generator over a synthetic resources/ tree
    and verify each output file is well-formed."""

    def setUp(self) -> None:
        self._td = tempfile.TemporaryDirectory()
        self.addCleanup(self._td.cleanup)
        self.root = Path(self._td.name)
        self.resources = self.root / "resources"
        self.outdir    = self.root / "out"
        self.resources.mkdir()
        self.outdir.mkdir()
        # Seed a small, diverse set: ASCII path, nested path, one with
        # an apostrophe (the original regression).
        _seed(self.resources, "items.xml",        b"<items/>\n")
        _seed(self.resources, "fonts/title.ttf",  b"\x00\x01TTF")
        _seed(self.resources, "art/joe's.png",    b"\x89PNG")

    def test_ids_header_defines_one_macro_per_file(self) -> None:
        files = gen.gather_resources(self.resources)
        gen.write_ids_header(self.outdir / "embedded_resources_ids.h", files)
        text = (self.outdir / "embedded_resources_ids.h").read_text()
        # 3 files -> 3 #defines starting at 1001.
        self.assertIn("#define HH_IDR_0001 1001", text)
        self.assertIn("#define HH_IDR_0002 1002", text)
        self.assertIn("#define HH_IDR_0003 1003", text)
        # Guard macros present.
        self.assertIn("#ifndef HH_EMBEDDED_RESOURCES_IDS_H", text)
        self.assertIn("#define HH_EMBEDDED_RESOURCES_IDS_H", text)
        self.assertIn("#endif", text)

    def test_rc_file_quotes_paths_with_apostrophes_verbatim(self) -> None:
        files = gen.gather_resources(self.resources)
        gen.write_rc_file(self.outdir / "embedded_resources.rc",
                          files, self.resources)
        text = (self.outdir / "embedded_resources.rc").read_text()
        # The apostrophe lives inside the quoted path literal -- no
        # escaping needed, no doubling, just a raw '. rc.exe handles it.
        self.assertIn("joe's.png", text)
        # Each entry uses RCDATA + a double-quoted path.
        for macro in ("HH_IDR_0001", "HH_IDR_0002", "HH_IDR_0003"):
            self.assertIn(f"{macro} RCDATA \"", text)

    def test_table_cpp_keys_use_posix_separators(self) -> None:
        files = gen.gather_resources(self.resources)
        gen.write_table_cpp(self.outdir / "embedded_resources_table.cpp", files)
        text = (self.outdir / "embedded_resources_table.cpp").read_text()
        # Even though Path.as_posix is used during generation, double-
        # check: no backslashes in the key strings (would break the
        # runtime lookup on a re-checkout from Windows).
        self.assertNotIn("\\\\", text)
        # Apostrophe is escaped via \" only when needed; for ' the C++
        # literal doesn't require escaping, so it appears raw.
        self.assertIn("joe's.png", text)
        self.assertIn("fonts/title.ttf", text)
        self.assertIn("items.xml", text)

    def test_portable_cpp_contains_bytes_table_and_lookup(self) -> None:
        files = gen.gather_resources(self.resources)
        out = self.outdir / "embedded_resources_portable.cpp"
        gen.write_portable_cpp(out, files, self.resources)
        text = out.read_text()

        self.assertIn("alignas(16) const unsigned char kData0[]", text)
        self.assertIn("0x89, 0x50, 0x4e, 0x47", text)
        self.assertIn("fonts/title.ttf", text)
        self.assertIn("items.xml", text)
        self.assertIn("bool find(const char* posix_relative_path", text)
        self.assertNotIn('#include "embedded_resources_ids.h"', text)


if __name__ == "__main__":
    unittest.main()
