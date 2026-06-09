"""Unit tests for tools/build_atlas.py.

Runs via either pytest (`pytest tests/test_build_atlas.py`) or directly as a
script (`python tests/test_build_atlas.py`). Skips itself with a clear
message if Pillow isn't installed -- the packer is a developer tool, not a
runtime dependency, so it's reasonable for CI to skip it on slim images.
"""
from __future__ import annotations

import os
import sys
import unittest

# Make the tools/ directory importable so we can exercise the packer.
_REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(_REPO_ROOT, "tools"))

try:
    from PIL import Image  # noqa: F401
    _HAVE_PIL = True
except ImportError:
    _HAVE_PIL = False


@unittest.skipUnless(_HAVE_PIL, "Pillow not installed; skipping atlas tests.")
class ShelfPackerTests(unittest.TestCase):
    """Direct tests for the shelf packer in build_atlas.py."""

    def setUp(self) -> None:
        import build_atlas
        self.build_atlas = build_atlas
        self.Image = Image  # type: ignore[name-defined]

    def _img(self, w: int, h: int):
        return self.Image.new("RGBA", (w, h), (0, 0, 0, 0))

    def test_empty_input_produces_zero_size(self) -> None:
        placements, w, h = self.build_atlas.pack_shelves([], padding=0, max_size=64)
        self.assertEqual(placements, {})
        self.assertEqual(w, 0)
        self.assertEqual(h, 0)

    def test_single_image_fits_exactly(self) -> None:
        placements, w, h = self.build_atlas.pack_shelves(
            [("a", self._img(16, 16))], padding=0, max_size=64,
        )
        self.assertIn("a", placements)
        x, y, ww, hh = placements["a"]
        self.assertEqual((x, y, ww, hh), (0, 0, 16, 16))
        self.assertEqual((w, h), (16, 16))

    def test_padding_grows_atlas(self) -> None:
        placements, w, h = self.build_atlas.pack_shelves(
            [("a", self._img(16, 16))], padding=2, max_size=64,
        )
        x, y, ww, hh = placements["a"]
        # Padding is added around the source; placement records the inner box.
        self.assertEqual((x, y), (2, 2))
        self.assertEqual((ww, hh), (16, 16))
        self.assertEqual((w, h), (20, 20))

    def test_two_sources_share_a_shelf(self) -> None:
        placements, w, h = self.build_atlas.pack_shelves(
            [("a", self._img(16, 16)), ("b", self._img(16, 16))],
            padding=0, max_size=64,
        )
        self.assertEqual(placements["a"][1], placements["b"][1])  # same y
        self.assertNotEqual(placements["a"][0], placements["b"][0])  # different x
        self.assertEqual(h, 16)

    def test_wider_than_max_overflows_to_next_shelf(self) -> None:
        placements, w, h = self.build_atlas.pack_shelves(
            [
                ("a", self._img(40, 20)),
                ("b", self._img(40, 20)),
                ("c", self._img(40, 10)),
            ],
            padding=0, max_size=64,
        )
        # a + b can't share a shelf (80 > 64), so 'b' wraps to a new row.
        self.assertEqual(placements["a"][1], 0)
        self.assertEqual(placements["b"][1], 20)
        # Tallest-first ordering keeps c on the second shelf with b
        # (it's shorter, so it sits next to b).
        self.assertGreater(h, 20)
        self.assertLessEqual(h, 64)
        self.assertLessEqual(w, 64)

    def test_oversized_source_raises(self) -> None:
        with self.assertRaises(RuntimeError):
            self.build_atlas.pack_shelves(
                [("huge", self._img(100, 100))], padding=0, max_size=64,
            )

    def test_atlas_overflow_raises(self) -> None:
        # Two 40x40 entries can't both fit in a 64x64 atlas (the second one
        # has to start at y=40, leaving only 24 rows -- which is OK -- but
        # a third 40x40 entry would force the cursor past max_size).
        with self.assertRaises(RuntimeError):
            self.build_atlas.pack_shelves(
                [
                    ("a", self._img(40, 40)),
                    ("b", self._img(40, 40)),
                    ("c", self._img(40, 40)),
                ],
                padding=0, max_size=64,
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
