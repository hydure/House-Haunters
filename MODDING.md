# Modding House Haunters

The game ships with a single mod file: [`resources/mods.xml`](resources/mods.xml).
Edit it, save, and re-launch — no rebuild required. The whole file is parsed at
startup; if it is missing, unreadable, or malformed the engine falls back to the
vanilla values and the game still boots.

> **Modding requires a dev build.** The single-file standalone `HH.exe`
> produced by [`_package_standalone.ps1`](_package_standalone.ps1) has the
> entire `resources/` tree (including `mods.xml`) baked into the executable
> via `RT_RCDATA` (see [README.md](README.md#how-the-resource-bundling-works))
> — there is no `resources/` folder to edit. To mod, clone the repo, edit
> files under `resources/`, and run the dev build (`_build_release.bat` or
> `_build_tests.bat`). When you're happy, re-run `_package_standalone.ps1`
> to bake your changes into a fresh single-file `HH.exe`.

This file controls four things:

1. **The character-selection lobby** — title text, fonts, colors, layout, and
   which portrait atlas is sliced for each character.
2. **Each playable character** — base HP, speed, which slot of the spritesheet
   the walk frames come from, and an optional per-character spritesheet
   override.
3. **The villain** — spritesheet, frame size, walk-animation frame indices, and
   starting HP.
4. **Audio & music** — the seven in-game cues (title loop, hunt cue,
   game-over sting, lobby thunder, hurt / death stings, ghost chase) can be
   pointed at any `.ogg` / `.flac` / `.wav` file under `resources/`.

> **Behavioral specials stay hardcoded.** MOM's worthless-clue upgrade,
> BRO's sprint toggle, SIS's stationary-stealth, and DAD's jackpot bonus
> damage are tied to the character identity in code. Mods can change stats
> and art, not abilities.

---

## File layout

All paths inside `mods.xml` are resolved **relative to `resources/`**, so
`sprites/character_sheet.png` means `resources/sprites/character_sheet.png`.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<mods>
    <screen> ... </screen>
    <characters>
        <character id="BRO" .../>
        <character id="SIS" .../>
        <character id="DAD" .../>
        <character id="MOM" .../>
    </characters>
    <villain ...> <walk .../> ... </villain>
    <audio>
        <track id="gameplay_music" path="music/start.ogg"/>
        ...
    </audio>
</mods>
```

You only need to include the sections / attributes you want to **change**.
Anything you omit keeps its default — partial mods are first-class.

---

## 1. The character-selection lobby (`<screen>`)

| Element | Attribute | Default | Notes |
|---|---|---|---|
| `<title>` | `text` | `MAKE YOUR TEAM` | The line drawn at the top of the lobby |
| | `font` | `fonts/Underdog-Regular.ttf` | Any `.ttf` under `resources/` |
| | `size` | `24` | Pixels |
| | `x`, `y` | `250, 50` | Top-left anchor |
| `<background>` | `r`, `g`, `b` | `30, 30, 30` | 0–255, clamped if you go outside |
| `<portraits>` | `atlas` | `HH_Portraits.png` | Texture to slice for portraits |
| | `tile_width` | `256` | Pixels per portrait in the atlas |
| | `tile_height` | `384` | |
| | `columns` | `4` | How many tiles wide the atlas is |
| | `scale` | `0.586` | Render scale (`0.586 ≈ 150×225` on screen) |
| `<layout>` | `start_x` | `40` | Leftmost portrait's x position |
| | `start_y` | `120` | Row y position |
| | `x_spacing` | `165` | Horizontal gap between portraits |

### Example — re-skin the lobby

```xml
<screen>
    <title text="PICK A VICTIM" font="fonts/myhorror.ttf" size="32" x="180" y="40"/>
    <background r="10" g="0" b="20"/>
    <portraits atlas="my_portraits.png" tile_width="200" tile_height="300"
               columns="4" scale="0.75"/>
    <layout start_x="20" start_y="140" x_spacing="180"/>
</screen>
```

---

## 2. Playable characters (`<characters>`)

Each `<character>` element targets exactly one of the four characters by `id`:

| `id` | In-game name | Hardcoded ability |
|---|---|---|
| `BRO` | Brother | Sprint toggle (X button) |
| `SIS` | Sister  | Invisible to the villain while stationary |
| `DAD` | Father  | Extra damage with jackpot-item pickups |
| `MOM` | Mother  | Can upgrade "worthless" clues |

(`id` is case-insensitive; you can also use the numeric index `0`–`3`.)

| Attribute | Default (BRO / SIS / DAD / MOM) | Notes |
|---|---|---|
| `hp` | `3 / 3 / 5 / 3` | Starting HP and, if `max_hp` is omitted, also the cap |
| `max_hp` | _falls back to_ `hp` | HP cap if you want it different from starting HP |
| `speed_multiplier` | `1.0 / 1.0 / 1.0 / 0.85` | Scales `Character::speed` (which defaults to 120) |
| `sprite_location` | `2 / 1 / 3 / 0` | Picks a 2×2 quadrant of the character spritesheet (see below) |
| `portrait_index` | `0 / 2 / 4 / 6` | Tile index into the portrait atlas (left-to-right, top-to-bottom) |
| `sprite_sheet` | `sprites/character_sheet.png` | Override per character if you give each one their own art |

### `sprite_location` decoded

The shared character spritesheet is a 2×2 grid of characters; each character's
animation frames live in one quadrant:

| `sprite_location` | quadrant | frame offset |
|---|---|---|
| `0` | top-left     | 0 |
| `1` | top-right    | 3 |
| `2` | bottom-left  | 24 |
| `3` | bottom-right | 27 |

Inside the chosen quadrant the engine always picks frames in this pattern:

| Direction | Frames |
|---|---|
| down  | `mod+1, mod+2, mod+1, mod+0` |
| left  | `mod+7, mod+8, mod+7, mod+6` |
| right | `mod+13, mod+14, mod+13, mod+12` |
| up    | `mod+19, mod+20, mod+19, mod+18` |

…where `mod = 3·x + 24·y` for the quadrant at `(x, y)` (so `0, 3, 24, 27`
respectively). If you supply your own `sprite_sheet`, it must follow the same
24-frame-per-quadrant layout.

### Example — give DAD his own atlas and make MOM tougher

```xml
<characters>
    <character id="DAD" sprite_sheet="sprites/dad_skeleton.png" sprite_location="0"/>
    <character id="MOM" hp="6" max_hp="6" speed_multiplier="1.0"/>
</characters>
```

---

## 3. The villain (`<villain>`)

| Attribute | Default | Notes |
|---|---|---|
| `sprite_sheet` | `sprites/ghost.png` | Any texture under `resources/` |
| `frame_width`  | `32` | Per-frame pixel size |
| `frame_height` | `48` | |
| `health`       | `10` | Starting HP |

Each `<walk>` line picks four (or more) frame indices into the spritesheet for
one direction:

| `direction` | default `frames` |
|---|---|
| `down`  | `1,2,1,0` |
| `left`  | `4,5,4,3` |
| `right` | `7,8,7,6` |
| `up`    | `10,11,10,9` |

You can pass any number of frames per direction (e.g. `0,1,2,3,4` for a
five-frame loop). Whitespace inside the CSV is ignored. An empty `frames=""`
attribute leaves that direction at its default rather than producing a
zero-frame animation.

### Example — replace the ghost with a zombie

```xml
<villain sprite_sheet="sprites/zombie.png"
         frame_width="48" frame_height="64"
         health="20">
    <walk direction="down"  frames="0,1,2,1"/>
    <walk direction="left"  frames="4,5,6,5"/>
    <walk direction="right" frames="8,9,10,9"/>
    <walk direction="up"    frames="12,13,14,13"/>
</villain>
```

---

## 4. Audio & music (`<audio>`)

Every in-game music loop and sound effect can be rebound to a different
file under `resources/`. The `<audio>` block holds one `<track>` per cue
you want to override; cues you don't list keep their vanilla path.

```xml
<audio>
    <track id="title_music"    path="music/your_loop.ogg"/>
    <track id="gameplay_music" path="music/start.ogg"/>
    <track id="endgame_music"  path="music/gameover.flac"/>
    <track id="lobby_select"   path="music/thunder.flac"/>
    <track id="player_hurt"    path="music/hurt.wav"/>
    <track id="player_death"   path="music/dead.wav"/>
    <track id="ghost_chase"    path="music/chase.wav"/>
</audio>
```

| `id` | Vanilla path | When it plays |
|---|---|---|
| `title_music`    | _(empty -- silent)_   | Looping background on the title screen. Vanilla ships silent; supply your own loop here to re-enable it. |
| `gameplay_music` | `music/start.ogg`     | "The hunt begins" cue at the start of a round. |
| `endgame_music`  | `music/gameover.flac` | Game-over / victory sting. |
| `lobby_select`   | `music/thunder.flac`  | Thunder crack when a player locks in their character. |
| `player_hurt`    | `music/hurt.wav`      | Played whenever a hero takes damage. |
| `player_death`   | `music/dead.wav`      | Played when a hero dies. |
| `ghost_chase`    | `music/chase.wav`     | Ghost lunge cue. |

**Supported formats:** `.ogg`, `.flac`, `.wav`. SFML 2.x does NOT ship
`libmpg123`, so `.mp3` files will fail to load (silent at runtime).

**Failure mode:** Each track loads independently. A typo or a missing file
in one `<track>` line silences only that single cue — the rest of the
game's audio still plays. Unknown `id` values are ignored (so adding a
future cue won't break older mod files).

**Tip:** Drop your replacement audio anywhere under `resources/` — for
example a `resources/music/custom/` subfolder — and reference it with the
same relative form (`music/custom/my_intro.ogg`).

### Example — silent-film mode

```xml
<audio>
    <!-- Swap the background music for a piano roll, leave SFX alone. -->
    <track id="title_music"    path="music/piano_intro.ogg"/>
    <track id="gameplay_music" path="music/piano_hunt.ogg"/>
    <track id="endgame_music"  path="music/piano_end.ogg"/>
</audio>
```

---

---

## Where to put your art

Drop replacement PNGs anywhere under `resources/` and reference them by their
path relative to that folder:

```text
resources/
    sprites/
        zombie.png            <-- referenced as "sprites/zombie.png"
        my_character.png
    my_portraits.png          <-- referenced as "my_portraits.png"
    music/
        custom/
            my_intro.ogg      <-- referenced as "music/custom/my_intro.ogg"
```

There is no compile or asset-bundle step; the game reads PNGs and audio
straight off disk via SFML.

---

## Verifying your mod

1. Save `resources/mods.xml`.
2. Launch the game — the lobby and first frame of gameplay will use the new
   values immediately. If a value is "missing" in-game, it usually means a
   typo in the attribute name; check the title text first (if the title is
   unchanged from `MAKE YOUR TEAM`, the whole `<screen>` block was probably
   ignored).
3. The test suite includes a regression check that proves
   `resources/mods.xml` parses cleanly and re-creates the vanilla values.
   You can run it without launching the full game:

   ```powershell
   .\build-ninja\tests\test_mod_config.exe
   ```

4. If you have a malformed file, the game still launches with vanilla
   defaults — there is no in-game error reporting (this is intentional so
   broken mods can't lock players out). Inspect `ModConfig::lastError()` if
   you are debugging from code.

---

## What's NOT (yet) moddable

- Room art (`resources/roompng/*.png`) and room layouts.
- Item / clue definitions (`resources/items.xml`).
- Hardcoded character abilities (MOM upgrade, BRO sprint, SIS stealth, DAD
  jackpot damage).
- Character spawn positions and the spawn-room selection RNG.

These are deliberate non-goals for the first modding pass. If you need any of
them, the `ModConfig` class in [include/engine/ModConfig.hpp](include/engine/ModConfig.hpp)
is the natural place to extend.
