# Platform services

The portable core owns filesystem conventions instead of deriving paths in
feature code. All paths are represented as `std::filesystem::path`; text at API
boundaries is validated UTF-8.

## Application directories

| Platform | Data | Configuration |
| --- | --- | --- |
| Windows | `%APPDATA%\Pokabbie\Rogue Assistant` | same as data |
| macOS | `~/Library/Application Support/Pokabbie/Rogue Assistant` | same as data |
| Linux | `$XDG_DATA_HOME/pokabbie/rogue-assistant`, or `~/.local/share/pokabbie/rogue-assistant` | `$XDG_CONFIG_HOME/pokabbie/rogue-assistant`, or `~/.config/pokabbie/rogue-assistant` |

Logs are stored below the data directory in `logs/RogueAssistant.log`. Exported
mGBA scripts use the data directory's `scripts` child. Installed resources are
resolved next to the executable in `resources`, or in the standard `Resources`
directory of a macOS application bundle.

On Windows, the first launch imports `%APPDATA%\.pokabbie\rogue_assistant`
and a legacy working-directory `settings.ini` only when the new application
directory is absent. Data is copied through a temporary sibling and renamed,
so an interrupted import does not expose a partial destination. The source is
never removed.

## Settings

`settings.ini` recognizes exactly these keys:

```ini
Multiplayer.HostPort=30025
Multiplayer.JoinIP=
Bridge.Port=30125
```

Ports must be decimal integers from 1 through 65535. Join addresses must be
valid UTF-8, contain no control characters, and be at most 255 bytes. Duplicate
keys, unknown keys, malformed lines, and oversized files are rejected. An
invalid settings file is logged and left unchanged; defaults apply only to the
current run. Successful writes use a flushed temporary sibling followed by an
atomic replacement.

The `--bridge-port` command-line override is applied by the application layer
and does not mutate `settings.ini`.
