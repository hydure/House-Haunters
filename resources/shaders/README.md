# resources/shaders/

GLSL shader sources loaded by SFML's `sf::Shader`. Paths are resolved
with [Paths::resource()](../../include/engine/Paths.hpp).

## Contents

| File | Purpose |
|---|---|
| `GradientShader.txt` | Fragment shader used for background fades and the lobby gradient. |
| `VertexShader.txt` | Companion vertex shader for the gradient pass. |

The `.txt` extension is historical (SFML doesn't care about extensions);
the contents are plain GLSL.

## Loading example

```cpp
sf::Shader shader;
shader.loadFromFile(Paths::resource("shaders/VertexShader.txt"),
                    Paths::resource("shaders/GradientShader.txt"));
```

## Adding a new shader

1. Drop the GLSL source here (any extension works; `.txt` is the
   established convention in this repo).
2. Load via `sf::Shader::loadFromFile(...)` with paths from
   `Paths::resource(...)`.
3. Confirm `sf::Shader::isAvailable()` before relying on shader-only
   visual effects — some headless test environments lack a usable
   shader pipeline.
