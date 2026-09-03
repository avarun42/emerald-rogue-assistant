# Platform services

The portable core owns filesystem conventions instead of deriving paths in
feature code. All paths are represented as `std::filesystem::path`; text at API
boundaries is validated UTF-8.

## Application directories

- **Windows data and configuration:**
  `%APPDATA%\avarun42\Rogue Assistant`
- **macOS data and configuration:**
  `~/Library/Application Support/avarun42/Rogue Assistant`
- **Linux data:** `$XDG_DATA_HOME/avarun42/rogue-assistant`, or
  `~/.local/share/avarun42/rogue-assistant` when `XDG_DATA_HOME` is not set
- **Linux configuration:** `$XDG_CONFIG_HOME/avarun42/rogue-assistant`, or
  `~/.config/avarun42/rogue-assistant` when `XDG_CONFIG_HOME` is not set

Rogue Assistant stores logs in `logs/RogueAssistant.log` below the data
directory. It stores exported mGBA scripts in the data directory's `scripts`
subdirectory. It resolves installed resources from the `resources` directory
next to the executable or the standard `Resources` directory in a macOS app
bundle.

On Windows, the first launch imports `%APPDATA%\.pokabbie\rogue_assistant` and
a legacy working-directory `settings.ini` only when the new application
directory is absent. Rogue Assistant copies the data to a temporary sibling
and then renames it, so an interrupted import does not expose a partial
destination. It never removes the source.

## Settings

`settings.ini` recognizes exactly these keys:

```ini
Multiplayer.HostPort=30025
Multiplayer.JoinIP=
Bridge.Port=30125
```

Ports must be decimal integers from 1 through 65535. Join addresses must be
valid UTF-8, contain no control characters, and be at most 255 bytes. Rogue
Assistant rejects duplicate keys, unknown keys, malformed lines, and oversized
files. It logs an invalid settings file and leaves the file unchanged. Default
values apply only to the current run. For a successful write, Rogue Assistant
flushes a temporary sibling and then atomically replaces the settings file.

The `--bridge-port` command-line option applies only to the current run. It does
not change `settings.ini`.
