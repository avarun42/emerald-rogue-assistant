# Architecture

Emerald Rogue Assistant is one desktop app and one mGBA Lua script. The app
contains all game, Home Box, multiplayer, storage, and user interface logic.
The Lua script only moves memory requests between mGBA and the app.

## Main parts

| CMake target | Purpose |
| --- | --- |
| `rogue_core` | Common data types, files, settings, logs, and ROM data |
| `rogue_bridge` | Memory requests and the local TCP bridge |
| `rogue_multiplayer` | ENet network code and multiplayer checks |
| `rogue_app` | Game sessions, Home Box, and app state |
| `RogueAssistant` | SFML window and process entry point |
| `rogue_tests` | Unit and behavior tests |

## Data flow

```text
mGBA and Emerald Rogue                 Emerald Rogue Assistant
----------------------                 ---------------
Lua frame callback                     main thread: window and input
readRange and write8/16/32      TCP     worker thread: game session
bounded memory work           <----->  Home Box, files, and multiplayer
```

The bridge listens only on `127.0.0.1`. It accepts one mGBA script at a time.
The script reconnects when mGBA resets or the app restarts.

## ROM data

Emerald Rogue Assistant does not parse or patch the ROM file. It reads the
running game's memory.

The app first reads the standard Game Boy Advance header at `0x08000100`. It
then checks the Emerald Rogue values and follows the pointer to the Rogue
Assistant header. This header provides the addresses, sizes, offsets, box
count, and player count used by the app.

The app requires ROM Assistant API 3. It also checks the ROM edition. Value `0`
means Vanilla and value `1` means EX. All values read from the ROM are checked
before they are used as an address, size, or array index.

## Memory requests

`GameSession` gives each read or write a nonzero 32-bit request ID. It keeps the
matching callback until the bridge returns a result. The worker thread runs all
callbacks.

The request rules are:

- No more than 256 requests can be waiting.
- One read or write can contain at most 1 MiB.
- Reads can use EWRAM, IWRAM, palette RAM, VRAM, OAM, or cartridge ROM.
- Writes can use only EWRAM or IWRAM.
- A request cannot be empty, cross a memory region, or overflow an address.
- A read result must have the requested size.
- A write result must have no data.

When the bridge is full or mGBA is paused, the app stops sending new
observation requests. It waits for room instead of growing a queue without a
limit.

`TcpLuaTransport` owns the listener and socket. It uses nonblocking calls. The
worker thread handles accepts, reads, protocol messages, and partial sends.
There is no network thread and no time limit for an idle connection.

## Thread ownership

The process main thread creates the SFML window, reads input, and draws each
frame. One `SessionWorker` thread owns all changing game state, network state,
and file work.

The UI sends plain `UiCommand` values to the worker. The worker gives the UI a
copied `UiSnapshot`. UI code cannot read or change a game behavior object.

At most 256 UI commands can wait. During shutdown, the worker:

1. Stops accepting commands.
2. Stops the bridge.
3. Disconnects the game.
4. Detaches each behavior and finishes any active save.
5. Saves settings and closes multiplayer.
6. Publishes its last UI state.
7. Joins its thread before the window is destroyed.

The same steps run after an exception. The last UI state contains the error.

## Files and resources

Platform code chooses the data, settings, and resource folders. Feature code
uses those paths and does not build its own platform path.

The app uses `std::filesystem::path` for paths and checks UTF-8 text at system
boundaries. Settings and Home Box data use temporary files and atomic rename
operations so that an interrupted write does not expose a partial file.

On macOS, resources are in `RogueAssistant.app/Contents/Resources`. On Windows
and Linux, the `resources` folder is beside the app. User paths are listed in
[Install Emerald Rogue Assistant](installation.md#user-files).

## Original Windows design

The original app wrote a Lua script that loaded `RogueAssistant.dll` inside
mGBA. The DLL created the SFML window and used shared queues to reach mGBA's
Lua thread.

The current app no longer builds or loads that DLL. The local TCP bridge now
uses the same design on every supported system. The game-facing C++ behavior
was kept and moved behind the new memory transport.
