# Installation and first run

Rogue Assistant is distributed separately from Pokemon Emerald Rogue and from
mGBA. It does not contain a ROM. Use only a ROM that you are legally entitled
to use.

## Requirements

- mGBA 0.10.5 or newer with Lua scripting enabled
- an Emerald Rogue Vanilla or EX ROM exposing Assistant API 3
- Windows x64, macOS 11 or newer on Apple silicon, or Linux x86_64
- one free loopback TCP port, `30125` by default

The bridge protocol is local-only. Multiplayer separately uses the configured
ENet host port (`30025` by default) and may require an operating-system firewall
rule for the player hosting a session.

## Verify a download

Every release contains `SHA256SUMS`. Verify the artifact before opening it:

```powershell
# Windows PowerShell
Get-FileHash .\RogueAssistant-1.0.0-windows-x64.zip -Algorithm SHA256
```

```sh
# macOS
shasum -a 256 RogueAssistant-1.0.0-macos-arm64.dmg

# Linux
sha256sum RogueAssistant-1.0.0-linux-x86_64.AppImage
```

Compare the entire hexadecimal value with the matching line in
`SHA256SUMS`. A mismatch means the file must not be used.

## Install

### Windows x64

Extract `RogueAssistant-1.0.0-windows-x64.zip` to a user-writable directory and
run `bin\RogueAssistant.exe`. Keep the `resources` directory beside the
executable. Moving only the executable produces a resource-loading failure.
The release binary statically links its Microsoft C/C++ runtime and does not
require a separate Visual C++ Redistributable installation.

### macOS

Open `RogueAssistant-1.0.0-macos-arm64.dmg` and drag
`RogueAssistant.app` to Applications. The release candidate is Apple silicon
native and contains only an arm64 slice. Public artifacts should be signed
and notarized; CI also creates an ad-hoc-signed development artifact when Apple
credentials are unavailable, which macOS will identify as a development build.

### Linux x86_64

For the AppImage:

```sh
chmod +x RogueAssistant-1.0.0-linux-x86_64.AppImage
./RogueAssistant-1.0.0-linux-x86_64.AppImage
```

Alternatively, extract `RogueAssistant-1.0.0-linux-x86_64.tar.gz` into an empty
directory and run `bin/RogueAssistant` without separating it from
`bin/resources`. The archive build uses desktop OpenGL/X11 system interfaces;
use the AppImage when those runtime dependencies are not already available.

## Connect to mGBA

1. Start Rogue Assistant. It exports the mGBA script to the application data
   folder and waits for mGBA on port `30125`.
2. Open the API-3 Emerald Rogue ROM in mGBA.
3. In mGBA, open **Tools > Scripting**.
4. Choose **File > Load Script** and select the exported
   `RogueAssistant_mGBA.lua` shown by Rogue Assistant.
5. Confirm that the assistant changes from waiting to connected.

On the waiting screen, `E` exports the script again, `C` copies its path, `R`
opens its folder, and `P` changes the saved connection port. If another program
already owns the port, choose the same replacement port in Rogue Assistant and
reload the newly exported Lua file in mGBA. The script reconnects automatically
after normal application restarts and ROM resets.

The full command line is:

```text
RogueAssistant [--bridge-port PORT] [--version] [--help]
```

`--bridge-port` is a one-run override. It does not modify `settings.ini`.

## User data and removal

Settings, logs, exported scripts, and Home Box data live in the documented
platform directories in [platform-services.md](platform-services.md). Removing
the application does not remove this data. Back it up before deleting it,
especially Home Box files and their `.bak` companions.

To uninstall, remove the extracted application or app bundle. Delete the data
and configuration directories separately only if their Home Box contents are
no longer needed.
