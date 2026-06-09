# include/game/screens/

Every `GameScreen` subclass — the units of "what is on screen right now."
The active screen owns the live `GameObject`s, listens for input events,
and asks `GameEngine` to switch to a different screen by name when the
user moves through the flow.

## Contents

| Header | Purpose |
|---|---|
| `GametitleScreen.hpp` | Title screen. Routes to Story / Character / Host / Join based on which button the user hit. |
| `GamestoryScreen.hpp` | Opening story screen. Fades from black, plays the intro scrolling text, then leads into Title. |
| `CharacterScreen.hpp` | Local-couch lobby. Up to four players join, each must pick a unique character. Title text, layout, portrait atlas, and per-character portrait index are all driven by [ModConfig](../../engine/ModConfig.hpp). `MENU` backs out to Title; `START` on the host (or auto when full) advances to GamePlay. |
| `ControlsScreen.hpp` | Static keymap reference reachable from the title screen. |
| `GameplayScreen.hpp` | The actual run. Builds the `RoomGroup`, instantiates one `Character` per joined player, places clues, spawns a `Villain`, and creates a `PlayerView` per player. Subscribes to `player_died` to count survivors and queue `change_screen("GameEnd")` when the last one falls. |
| `EndGameScreen.hpp` | `GAME OVER` / `YOU WIN` screen. Listens for any button to return to the title. |
| `HostScreen.hpp` | Networked-lobby host side. Listens on a TCP port, accepts up to N joiners (default 3), exposes a join code, and forwards to GamePlay when full or when the host force-starts. |
| `JoinScreen.hpp` | Networked-lobby client side. Asks for a join code, dials the host, completes the handshake, and forwards to GamePlay once the host starts the run. |
| `PauseMenu.hpp` | In-game pause overlay. Drawn on top of `GameplayScreen`; pauses simulation while open. |

## Screen lifecycle

Every screen overrides at least `init()`, `onUpdate(dt)`, and `onDraw(...)`,
all defined on the base [`GameScreen`](../../engine/GameScreen.hpp). The
contract:

* `init()` runs **every time** the screen is entered, not just the first
  time — there's a regression test for this in
  [tests/test_screen_flow.cpp](../../../tests/test_screen_flow.cpp).
* Subscribe to events via `this->subscribe(eventName, callback)`. The
  returned subscription is owned by the screen and released
  automatically on exit, so listeners do not leak across screens.
* Request a screen change by triggering the `change_screen` event with
  the name of the target screen:
  ```cpp
  Events::triggerEvent("change_screen",
      std::make_shared<Event<std::string>>("GamePlay"));
  ```

## Adding a new screen

1. Header here, implementation in [src/game/screens/](../../../src/game/screens/).
2. Subclass `GameScreen`.
3. Register the screen by name in
   [src/HouseHaunters.cpp](../../../src/HouseHaunters.cpp)
   (`this->addGameScreen("MyName", std::move(screen))`).
4. Add a `change_screen` event that routes to it from whichever existing
   screen kicks it off.
5. Write a `test_screen_flow`-style test exercising the new transition.
