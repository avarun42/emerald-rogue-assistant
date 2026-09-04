# Troubleshooting

## Rogue Assistant keeps waiting for mGBA

Check these items:

1. Use mGBA 0.10.5 or later.
2. Start an Emerald Rogue ROM with Assistant API 3.
3. Press `R` in Rogue Assistant to open the exported script folder.
4. Open **Tools > Scripting** in mGBA.
5. Load `RogueAssistant_mGBA.lua` from the exported script folder.
6. Make sure only one Rogue Assistant app uses that port and only one mGBA
   script is connected.

If you changed the bridge port, press `E` in Rogue Assistant to export the
script again. Reload that new script in mGBA.

## The port is already in use

Press `P` on the waiting screen. Enter a port from 1 to 65535, then press
Enter. Rogue Assistant keeps the old port if it cannot open the new one.

Export and reload the Lua script after the port changes.

## The ROM is not compatible

Rogue Assistant 1.0 requires ROM Assistant API 3. It accepts both the Vanilla
and EX editions. It rejects older APIs and unknown future APIs.

The ROM reports its API and memory layout after it starts. Rogue Assistant does
not inspect the `.gba` file itself.

## Storage transfer stopped

This message can appear if Rogue Assistant reconnects while Extended Storage
is already open in the game. Do not continue the old transfer. Press B. Then
open Extended Storage again.

This state stops Rogue Assistant from changing data from an old connection.

## Home Box could not load or save

Do not delete the Home Box files. Rogue Assistant leaves an invalid main file
unchanged and tries its backup. The app shows a warning when it loads a backup.

Copy the complete trainer folder before you try to repair anything. See
[Home Box file format](home-box-format.md) for file names and recovery rules.

## macOS blocks the app

An ad hoc signed beta can show an unknown developer warning. Follow the macOS
steps in [Install Rogue Assistant](installation.md#macos).

The current macOS package is for Apple silicon. Do not start it through Rosetta
or force the `x86_64` architecture.

## Find the log

The log file is named `RogueAssistant.log`:

- Windows: `%APPDATA%\Rogue Assistant\logs`
- macOS: `~/Library/Application Support/rogue.emerald.assistant/logs`
- Linux: `$XDG_DATA_HOME/rogue-assistant/logs`, or
  `~/.local/share/rogue-assistant/logs`

When you report a problem, include:

- Your operating system and CPU type
- The Rogue Assistant, mGBA, and Emerald Rogue versions
- The Vanilla or EX edition
- The ROM Assistant API version
- What you expected
- What happened
- The relevant log lines, after you check them for private data
