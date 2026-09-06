# Troubleshooting

## Emerald Rogue Assistant keeps waiting for mGBA

Check these items:

1. Use mGBA 0.10.5 or later.
2. Start Emerald Rogue 2.2.0 or later, in either the Vanilla or EX edition.
3. Press `R` in Emerald Rogue Assistant to open the exported script folder.
4. Open **Tools > Scripting** in mGBA.
5. Load `RogueAssistant_mGBA.lua` from the exported script folder.
6. Make sure only one Emerald Rogue Assistant app uses that port and only one mGBA
   script is connected.

If you changed the bridge port, press `E` in Emerald Rogue Assistant to export the
script again. Reload that new script in mGBA.

## The port is already in use

Press `P` on the waiting screen. Enter a port from 1 to 65535, then press
Enter. Emerald Rogue Assistant keeps the old port if it cannot open the new one.

Export and reload the Lua script after the port changes.

## The ROM is not compatible

Use Emerald Rogue 2.2.0 or later, in either the Vanilla or EX edition. If you
have just updated the game, check for an update to Emerald Rogue Assistant too.

## Storage transfer stopped

This message can appear if Emerald Rogue Assistant reconnects while Extended Storage
is already open in the game. Do not continue the old transfer. Press B. Then
open Extended Storage again.

This state stops Emerald Rogue Assistant from changing data from an old connection.

## Home Box could not load or save

Do not delete the Home Box files. Emerald Rogue Assistant leaves an invalid main file
unchanged and tries its backup. The app shows a warning when it loads a backup.

Copy the complete trainer folder before you try to repair anything. See
[Home Box file format](home-box-format.md) for file names and recovery rules.

## macOS blocks the app

An ad hoc signed beta can show an unknown developer warning. Follow the macOS
steps in [Install Emerald Rogue Assistant](installation.md#macos).

The current macOS package is for Apple silicon. Do not start it through Rosetta
or force the `x86_64` architecture.

## Find the log

The log file is named `RogueAssistant.log`:

- Windows: `%APPDATA%\Emerald Rogue Assistant\logs`
- macOS: `~/Library/Application Support/assistant.emerald.rogue/logs`
- Linux: `$XDG_DATA_HOME/emerald-rogue-assistant/logs`, or
  `~/.local/share/emerald-rogue-assistant/logs`

When you report a problem, include:

- Your operating system and CPU type
- The versions of Emerald Rogue Assistant, mGBA, and Emerald Rogue
- The Vanilla or EX edition
- The ROM Assistant API version
- What you expected
- What happened
- The relevant log lines, after you check them for private data
