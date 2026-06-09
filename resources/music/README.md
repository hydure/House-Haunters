# resources/music/

Looping background music and situational audio cues. Long files are
typically `.ogg` or `.flac` to keep the repo size reasonable; short
stings use `.wav`.

SFML plays these through `sf::Music` (streaming) or `sf::SoundBuffer` +
`sf::Sound` (loaded into memory) depending on the use site. Paths are
resolved with [Paths::resource()](../../include/engine/Paths.hpp).

## Contents

| File | Purpose |
|---|---|
| `chase.wav` | Played when the villain is actively pursuing a character. |
| `curse.wav` | Played when a wrong item is used on the villain. |
| `dead.wav` | Player-death sting. |
| `gameover.flac` | Background music for the `EndGame` screen. |
| `hunt.ogg` | Default exploration music while characters search the house. |
| `hurt.wav` | Sting played when a character takes damage. |
| `loud.wav` | Loud accent cue (e.g. door slam). |
| `near.flac` | Ramped-up music played when the villain is in an adjacent room. |
| `start.ogg` | Played as the run begins. |
| `thunder.flac` | Ambient thunder cue. |

## Adding audio

* Prefer `.ogg` for long stems, `.flac` for high-fidelity loops, and
  `.wav` for short one-shots.
* Reference the file through `Paths::resource("music/your_file.ext")`
  rather than hardcoding the prefix anywhere.
* Keep loudness consistent (-14 LUFS for stems, -10 LUFS for stings) so
  the mix doesn't whiplash between cues.

## Modding the audio

Players can rebind every in-game music loop and sound effect to a
different file without recompiling. Add `<track id="..." path="..."/>`
entries inside the `<audio>` block of [resources/mods.xml](../mods.xml);
see the [Audio & music](../../MODDING.md#4-audio--music-audio) section of
`MODDING.md` for the list of supported ids. A missing or malformed track
silences only that single cue — the rest of the audio still plays.

## Licensing

Confirm redistribution rights for every imported audio file. If a track
requires attribution, add it to the main [README.md](../../README.md).
