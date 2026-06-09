# resources/fonts/

TrueType fonts loaded at runtime via
[ResourceManager::getFont](../../include/engine/ResourceManager.hpp).
Paths are resolved through `Paths::resource(rel)` so the canonical use
site looks like:

```cpp
sf::Font* font = ResourceManager::getFont(
    Paths::resource("fonts/Underdog-Regular.ttf"));
```

## Contents

| File | Used by |
|---|---|
| `Underdog-Regular.ttf` | Default UI font (title, lobby, end-of-game text). The lobby's font is mod-overridable through `<screen title_font="..."/>` in [resources/mods.xml](../mods.xml). |
| `youmurderer.ttf` | Decorative title-screen font. |

## Adding a font

Drop the `.ttf` here and reference it through `Paths::resource(...)` from
the screen that needs it. `ResourceManager` caches the result, so multiple
text labels sharing a font only load it once.

## Licensing

These fonts are third-party. Confirm the upstream license permits
redistribution before committing a new file here. If a font's license
requires attribution, add it to the main [README.md](../../README.md).
