# 2009tool

Injected DLL for the Roblox 2009E client. ImGui overlay drawn directly into
the game's D3D9 backbuffer — no external window, no focus juggling.

## Build

Run `build.bat` — it auto-locates the VS-bundled cmake and builds both targets.

Requires:
- Visual Studio with the **C++ workload** and **C++ CMake tools for Windows** component
- [DirectX SDK (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=6812) installed at its default path

Outputs `internal.dll` and `injector.exe` in the repo root.

## Use

1. Launch `RobloxApp_client.exe` and join a place.
2. Run `injector.exe` — it auto-finds `internal.dll` in the same directory.
3. Press **INSERT** to toggle the overlay.

## Features

### ESP tab
- **Box ESP** — full, corner, or 3D box styles; optional gradient and filled variants with adjustable opacity
- **Health bar** — with optional full→empty color gradient
- **Name label** — per-player name above the box
- **Distance** — shows distance to each player
- **Snaplines** — lines from screen bottom to each player
- **Off-screen arrows** — directional indicators with adjustable size for players outside the view
- All colors are individually configurable via color pickers
- **Save / Load / Reset** config buttons (persisted to `esp_config.dat`)

### Executor tab
- Full Lua editor with syntax highlighting (ImGuiColorTextEdit)
- **Execute** button or **Ctrl+Enter** to run
- Scripts run on Roblox's own thread — no sync issues
- Scrolling log output; errors shown in red
- **Clear Script** and **Clear Log** buttons

### Explorer tab
- Live tree view of the DataModel (up to depth 6, 1024 nodes)
- Rebuilds every 2 s automatically
- Click a node to inspect it in the **Properties** panel on the right
- Double-click any node to copy its name to clipboard

### Properties panel
- Reads live property values via a runtime reflection decoder — walks the
  selected instance's class descriptor and decodes each getter's x86 thunk
  bytecode to determine field kind and offset
- Handles direct fields, indirect (via primary/secondary pointer), bitfield
  bools, vtable-relayed getters, std::string, Vector3, and the special
  `Position` chain
- `BrickColor` / `TeamColor` ints render with their human-readable name
- Properties grouped by category, with a count of decoded vs. total

## How it works

- `hooks.cpp` reads the D3D9 vtable via a throwaway device, patches `EndScene`
  (slot 42) and `Reset` (slot 16) with 5-byte JMP trampolines. Backbuffer
  dimensions are cached and only re-queried on `Reset`.
- `EndScene` runs every frame: ticks the Lua executor, draws ESP, draws the
  ImGui menu, then calls the original via trampoline.
- DirectInput8 `GetDeviceState`/`GetDeviceData` are patched on dummy ANSI and
  Unicode devices; all devices the game creates inherit the hooks. Input is
  fed to ImGui via `GetAsyncKeyState` polling on the render thread.
- All game-memory reads go through `SafeDeref` / `WalkChain` (SEH-wrapped)
  so a stale pointer or unmapped page can't crash the host process.

## Diagnostic build

`dumper.dll` is a standalone DLL that, when injected in place of `internal.dll`,
walks every loaded class descriptor and dumps property names and raw getter
bytes to `%TEMP%\rbx_dump.txt`. Useful for adding new getter patterns to the
reflection decoder.

## Files

```
src/
  dllmain.cpp     entry point + init thread
  hooks.cpp/h     D3D9 + DirectInput vtable hooks, WndProc subclass
  esp.cpp/h         player ESP (box, health, name, distance, snaplines, arrows)
  menu.cpp/h        ImGui tab bar (ESP / Executor / Explorer + Properties)
  executor.cpp/h    Lua queue + pcall loop + circular log buffer
  explorer.cpp/h    DataModel tree builder + ImGui tree renderer
  properties.cpp/h  live property table — reads values via the reflection decoder
  config.h          Config struct with binary save/load
  draw.h            ImGui draw primitives + WorldToScreen
  roblox.h          pointer chains, struct readers, SafeDeref, WalkChain,
                    PropKind enum + DecodePropGetter + EnumerateProperties
  lua_api.h         Lua function pointers resolved from hardcoded RVAs
  types.h           Vec3/Color/Camera shared types
  dumper.cpp        standalone diagnostic DLL (see "Diagnostic build" above)
injector/
  injector.cpp      CreateRemoteThread + LoadLibraryA loader
```
