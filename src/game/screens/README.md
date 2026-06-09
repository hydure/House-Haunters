# src/game/screens/

Implementations for every `GameScreen` subclass declared in
[include/game/screens/](../../../include/game/screens/). See that
folder's [README](../../../include/game/screens/README.md) for the
purpose of each screen and the screen-flow contract.

## Contents

| File | Header | Notes |
|---|---|---|
| `GametitleScreen.cpp` | [GametitleScreen.hpp](../../../include/game/screens/GametitleScreen.hpp) | Routes title button presses to Story / Character / Host / Join. |
| `GamestoryScreen.cpp` | [GamestoryScreen.hpp](../../../include/game/screens/GamestoryScreen.hpp) | Opening cutscene; fades + scrolling text. |
| `CharacterScreen.cpp` | [CharacterScreen.hpp](../../../include/game/screens/CharacterScreen.hpp) | Local-couch lobby. Reads title text, layout, portrait atlas path / tile size, columns, and start / spacing from `ModConfig::instance().screen()`; per-character portrait index from `.character(i).portrait_index`. Enforces unique character picks across players. |
| `ControlsScreen.cpp` | [ControlsScreen.hpp](../../../include/game/screens/ControlsScreen.hpp) | Static controls reference. |
| `GameplayScreen.cpp` | [GameplayScreen.hpp](../../../include/game/screens/GameplayScreen.hpp) | The main run. Builds the `RoomGroup`, characters, villain, clues, and per-player views; subscribes to `player_died` and routes to `EndGame` when no player is alive. |
| `EndGameScreen.cpp` | [EndGameScreen.hpp](../../../include/game/screens/EndGameScreen.hpp) | `GAME OVER` / `YOU WIN` screen. |
| `HostScreen.cpp` | [HostScreen.hpp](../../../include/game/screens/HostScreen.hpp) | Networked-lobby host side. Listens on the configured port, accepts up to `kMaxJoiners` clients, exposes the join code, and lets the host force-start with fewer than the maximum joiners. |
| `JoinScreen.cpp` | [JoinScreen.hpp](../../../include/game/screens/JoinScreen.hpp) | Networked-lobby client side. Accepts a join code, dials the host, completes the handshake, and waits for the host to advance. |
| `PauseMenu.cpp` | [PauseMenu.hpp](../../../include/game/screens/PauseMenu.hpp) | Overlay drawn on top of `GameplayScreen`. While open, gameplay updates pause. |

## Test coverage

Screen-flow regressions live in
[tests/test_screen_flow.cpp](../../../tests/test_screen_flow.cpp).
The networked-lobby host / join handshake has dedicated coverage in
[tests/test_network_host_join.cpp](../../../tests/test_network_host_join.cpp).
Spectator behavior at the GAME OVER threshold is covered by
[tests/test_spectator.cpp](../../../tests/test_spectator.cpp).
