# Standalone desktop application

The modern runtime is a normal desktop process. mGBA does not load native
Rogue Assistant code: it runs the bundled `RogueAssistant_mGBA.lua` adapter,
which connects to the application over loopback TCP.

## Starting a session

1. Start Rogue Assistant. It listens only on `127.0.0.1`, using port `30125`
   unless configured otherwise.
2. Start an API-3 Emerald Rogue Vanilla or EX ROM in mGBA 0.10.5 or newer.
3. Open **Tools > Scripting** in mGBA, choose **File > Load Script**, and load
   the exported `RogueAssistant_mGBA.lua` file shown by the application.

The application exports the script during startup. The script is stable under
the platform data directory's `scripts` child and contains the effective port
for that run. The disconnected screen provides these keyboard actions:

- `P`: edit and persist the bridge port
- `E`: export the script again
- `C`: copy the exported script path
- `R`: reveal the script directory in the platform file manager

Changing the configured port first binds a replacement listener. If binding
fails, the working listener and saved setting remain unchanged. A successful
change closes any current bridge session, atomically updates `settings.ini`,
exports a matching script, and waits for mGBA to reconnect.

## Command line

```text
RogueAssistant [--bridge-port PORT] [--version] [--help]
```

`--bridge-port` affects only the current process. Port selection is strictly:
the command-line value, then `Bridge.Port` in `settings.ini`, then `30125`.
Ports are decimal integers from 1 through 65535. Repeated or unknown options
are rejected.

The CMake project version supplies the window, UI, log, command-line, and
bridge-handshake application version. `RogueAssistant --version` therefore
reports the same version packaged in the application metadata.

## Resources

Standalone builds load the font, frame, window icon, and Lua source from
installed resources. On macOS these live in the application bundle's
`Contents/Resources`; Windows and Linux builds use a `resources` directory
beside the executable. The embedded resource path remains only in the temporary
legacy Windows DLL project until that parity reference is retired.
