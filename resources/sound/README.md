# resources/sound/

One-shot sound effects. Currently a placeholder — the existing audio
cues live next door in [../music/](../music/) and are loaded as either
streaming `sf::Music` or buffered `sf::Sound`s depending on length.

## Contents

| File | Purpose |
|---|---|
| `keep.txt` | Placeholder so Git tracks the empty directory. Safe to delete once a real `.wav` lands here. |

## Adding a sound

* Prefer `.wav` for short stings (decoded into a `sf::SoundBuffer`).
* Use `.ogg` if the clip is long enough that streaming via `sf::Music`
  is preferable.
* Load through [ResourceManager](../../include/engine/ResourceManager.hpp)
  so repeated playback doesn't re-decode from disk.

## Why is this folder separate from `music/`?

Historical split — music streams (one `sf::Music` per track) while
short SFX get loaded into memory once and reused (one `sf::SoundBuffer`,
many `sf::Sound`s). Either folder works for either purpose; once a real
SFX is added, prefer this folder for buffered cues and reserve
`music/` for streaming stems.
