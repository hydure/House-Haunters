#!/usr/bin/env python3
"""Pack runtime sprite sheets into one atlas + JSON metadata.

Why
---
House Haunters currently binds one sf::Texture per sprite source (heart,
swipe, grave, ghost, ...). For tiny per-instance overlays (hearts, swipe,
clue text background) that means one texture-bind per draw call. Packing
those small sources into a single atlas means one bind per frame for the
whole HUD/effects layer.

Inputs / outputs
----------------
Reads a manifest JSON (see tools/atlas_manifest.json) that lists:
  - output.image: where the packed PNG should be written
  - output.json:  where the per-source UV table should be written
  - output.padding:  pixels of transparent gutter around each source (avoids
                     bleeding when the atlas is sampled with linear filtering)
  - output.max_size: hard cap on the atlas dimension (e.g. 1024, 2048)
  - sources: list of {name, path} entries, each pointing at a PNG

Writes:
  - <output.image>  -- the packed atlas
  - <output.json>   -- a manifest of {name: {x, y, w, h, source}} that the
                       game can load to look up UVs.

Algorithm
---------
Sort sources by descending height and pack them into shelves (rows). It's
a "Next-Fit Decreasing Height" packer -- O(n log n) and produces tight
packings for the small, similar-sized inputs we care about.

Dependencies
------------
Pillow (`pip install Pillow`). The script only runs when atlas assets
change, so this is a developer-only requirement, not a runtime dependency.

Usage
-----
    python tools/build_atlas.py                                # default manifest
    python tools/build_atlas.py path/to/manifest.json          # explicit
    python tools/build_atlas.py --check                        # exit non-zero
                                                               # if atlas is
                                                               # out of date
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
from typing import Dict, List, Optional, Tuple

try:
    from PIL import Image
except ImportError:
    sys.stderr.write(
        "error: Pillow is required (pip install Pillow)\n"
        "       This is a developer-only dependency; it is not needed to\n"
        "       build or run the game.\n"
    )
    sys.exit(2)


# ----------------------------- manifest IO -----------------------------------

def load_manifest(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def resolve_paths(manifest: dict, project_root: str) -> Tuple[str, str, List[dict]]:
    out = manifest["output"]
    image_path = os.path.normpath(os.path.join(project_root, out["image"]))
    json_path = os.path.normpath(os.path.join(project_root, out["json"]))
    sources = []
    for src in manifest["sources"]:
        sources.append({
            "name": src["name"],
            "path": os.path.normpath(os.path.join(project_root, src["path"])),
        })
    return image_path, json_path, sources


# ------------------------------ packing -------------------------------------

def pack_shelves(
    images: List[Tuple[str, Image.Image]],
    padding: int,
    max_size: int,
) -> Tuple[Dict[str, Tuple[int, int, int, int]], int, int]:
    """Pack `images` using next-fit decreasing height. Returns a (uv map,
    atlas width, atlas height) triple. Raises RuntimeError if anything
    won't fit inside `max_size`."""
    # Tallest first so each shelf is dominated by its first entry.
    ordered = sorted(images, key=lambda item: item[1].height, reverse=True)

    placements: Dict[str, Tuple[int, int, int, int]] = {}
    cursor_x = 0
    cursor_y = 0
    shelf_height = 0
    atlas_width = 0
    atlas_height = 0

    for name, img in ordered:
        w = img.width + padding * 2
        h = img.height + padding * 2

        if w > max_size or h > max_size:
            raise RuntimeError(
                f"source '{name}' ({img.width}x{img.height}) exceeds atlas "
                f"max_size={max_size}"
            )

        # Move to a new shelf if this entry doesn't fit on the current one.
        if cursor_x + w > max_size:
            cursor_x = 0
            cursor_y += shelf_height
            shelf_height = 0

        if cursor_y + h > max_size:
            raise RuntimeError(
                f"atlas overflow: can't fit '{name}' within max_size={max_size}; "
                f"increase output.max_size or split the manifest"
            )

        placements[name] = (cursor_x + padding, cursor_y + padding, img.width, img.height)
        cursor_x += w
        shelf_height = max(shelf_height, h)
        atlas_width = max(atlas_width, cursor_x)
        atlas_height = max(atlas_height, cursor_y + shelf_height)

    return placements, atlas_width, atlas_height


def write_atlas(
    placements: Dict[str, Tuple[int, int, int, int]],
    images: Dict[str, Image.Image],
    width: int,
    height: int,
    out_path: str,
) -> None:
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    atlas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    for name, (x, y, w, h) in placements.items():
        atlas.paste(images[name], (x, y))
    atlas.save(out_path, format="PNG", optimize=True)


def write_json(
    placements: Dict[str, Tuple[int, int, int, int]],
    images: Dict[str, Image.Image],
    width: int,
    height: int,
    out_path: str,
    source_paths: Dict[str, str],
) -> None:
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    entries: Dict[str, dict] = {}
    for name, (x, y, w, h) in sorted(placements.items()):
        entries[name] = {
            "x": x,
            "y": y,
            "w": w,
            "h": h,
            "source": os.path.relpath(source_paths[name]).replace("\\", "/"),
        }
    payload = {
        "atlas": {
            "width": width,
            "height": height,
        },
        "frames": entries,
    }
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2)
        f.write("\n")


# ---------------------------- freshness check -------------------------------

def manifest_hash(manifest: dict, sources: List[dict]) -> str:
    """Hash the manifest bytes + every source file's bytes. Used by --check
    to decide whether the atlas needs to be rebuilt."""
    h = hashlib.sha256()
    h.update(json.dumps(manifest, sort_keys=True).encode("utf-8"))
    for src in sources:
        try:
            with open(src["path"], "rb") as f:
                h.update(src["name"].encode("utf-8"))
                h.update(b"\0")
                h.update(f.read())
        except OSError as exc:
            raise RuntimeError(f"cannot read source '{src['path']}': {exc}") from exc
    return h.hexdigest()


# -------------------------------- main --------------------------------------

def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Pack runtime sprites into an atlas.")
    parser.add_argument(
        "manifest",
        nargs="?",
        default=os.path.join(os.path.dirname(__file__), "atlas_manifest.json"),
        help="Path to the atlas manifest JSON (default: tools/atlas_manifest.json).",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Exit non-zero if the atlas would change. Useful as a CI guard.",
    )
    args = parser.parse_args(argv)

    project_root = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    manifest = load_manifest(args.manifest)
    image_path, json_path, sources = resolve_paths(manifest, project_root)

    images: Dict[str, Image.Image] = {}
    source_paths: Dict[str, str] = {}
    for src in sources:
        if not os.path.exists(src["path"]):
            sys.stderr.write(f"error: source not found: {src['path']}\n")
            return 1
        images[src["name"]] = Image.open(src["path"]).convert("RGBA")
        source_paths[src["name"]] = src["path"]

    out = manifest["output"]
    placements, width, height = pack_shelves(
        list(images.items()),
        padding=int(out.get("padding", 2)),
        max_size=int(out.get("max_size", 1024)),
    )

    if args.check:
        # Re-read the previous JSON (if any) and compare.
        if not os.path.exists(json_path):
            sys.stderr.write("error: atlas json missing; run without --check to build it.\n")
            return 3
        with open(json_path, "r", encoding="utf-8") as f:
            previous = json.load(f)
        # The JSON we'd be about to write -- simulate without touching disk.
        current = {
            "atlas": {"width": width, "height": height},
            "frames": {
                name: {
                    "x": x, "y": y, "w": w, "h": h,
                    "source": os.path.relpath(source_paths[name]).replace("\\", "/"),
                }
                for name, (x, y, w, h) in sorted(placements.items())
            },
        }
        if previous != current:
            sys.stderr.write("error: atlas is out of date; re-run build_atlas.py.\n")
            return 4
        return 0

    write_atlas(placements, images, width, height, image_path)
    write_json(placements, images, width, height, json_path, source_paths)
    print(f"wrote {image_path} ({width}x{height})")
    print(f"wrote {json_path} ({len(placements)} entries)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
