# Install Emerald Rogue Assistant

Emerald Rogue Assistant is separate from mGBA and Pokémon Emerald Rogue. Release
packages do not include a ROM.

## Before you install

You need:

- mGBA 0.10.5 or later with Lua support
- Emerald Rogue 2.2.0 or later (Vanilla or EX)
- Windows x64, macOS 11 or later on Apple silicon, or Linux x86_64
- A free local TCP port; the default is `30125`

Multiplayer uses a separate port. The default multiplayer port is `30025`.
The host might need to allow this port through a firewall.

## Check the download

Each release includes `SHA256SUMS`. Use it to check that your download is
complete and unchanged. In the commands below, replace `<build>` with the
label in your downloaded filename. Development packages use `dev-` followed
by a commit ID; named releases use a version such as `1.0.0-alpha.0`.

On Windows PowerShell:

```powershell
Get-FileHash '.\RogueAssistant-<build>-windows-x64.zip' -Algorithm SHA256
```

On macOS:

```sh
shasum -a 256 'RogueAssistant-<build>-macos-arm64.dmg'
```

On Linux:

```sh
sha256sum 'RogueAssistant-<build>-linux-x86_64.AppImage'
```

Compare the full value with the matching line in `SHA256SUMS`. Do not use the
file if the values differ.

## Windows

Extract `RogueAssistant-<build>-windows-x64.zip` to a folder where you can
write files. Run `bin\RogueAssistant.exe`.

Keep the `resources` folder beside the app. The app cannot start if you move
only the `.exe` file.

## macOS

Open `RogueAssistant-<build>-macos-arm64.dmg`. Drag
`RogueAssistant.app` to Applications.

The DMG contains only the app and an Applications shortcut. You do not need a
second ZIP download. GitHub Actions wraps development artifacts in a ZIP;
extract that download and open the DMG inside. Named GitHub releases provide
the DMG directly.

The macOS package runs only on Apple silicon. A package from GitHub Actions uses
an ad hoc signature unless the release job has Apple signing details. macOS
can show an unknown developer warning for an ad hoc signed build.

To open such a build:

1. Control-click `RogueAssistant.app` in Finder.
2. Select **Open**.
3. Select **Open** again in the warning.

If **Open** is not available, try to open the app once. Then go to **System
Settings > Privacy & Security** and select **Open Anyway**.

## Linux

To use the AppImage:

```sh
chmod +x 'RogueAssistant-<build>-linux-x86_64.AppImage'
'./RogueAssistant-<build>-linux-x86_64.AppImage'
```

You can also extract
`RogueAssistant-<build>-linux-x86_64.tar.gz`. Run
`bin/RogueAssistant` from the extracted files. Keep `bin/resources` beside the
app.

The archive uses OpenGL and X11 libraries from the system. Use the AppImage if
those libraries are not installed.

## Connect mGBA

1. Start Emerald Rogue Assistant. It exports its Lua script and waits for mGBA.
2. Open the supported Emerald Rogue ROM in mGBA.
3. Press `R` in Emerald Rogue Assistant to open the exported script folder.
4. In mGBA, select **Tools > Scripting**.
5. Select **File > Load Script**.
6. Open `RogueAssistant_mGBA.lua` from the exported script folder.
7. Wait for Emerald Rogue Assistant to show that mGBA is connected.

The waiting screen has these controls:

- `P`: change the saved connection port
- `E`: export the script again
- `C`: copy the script path
- `R`: open the script folder

The exported script contains the port that was active when it was created.
Export and reload the script after you change the port. The script reconnects
after an app restart or a ROM reset.

## Command line

```text
RogueAssistant [--bridge-port PORT] [--version] [--help]
```

`--bridge-port` changes the port for that run only. It does not change
`settings.ini`.

The app chooses the bridge port in this order:

1. The `--bridge-port` value
2. `Bridge.Port` in `settings.ini`
3. Port `30125`

## User files

Emerald Rogue Assistant stores settings, logs, the exported script, and Home Box data
in these folders:

- Windows: `%APPDATA%\Emerald Rogue Assistant`
- macOS: `~/Library/Application Support/assistant.emerald.rogue`
- Linux data: `$XDG_DATA_HOME/emerald-rogue-assistant`, or
  `~/.local/share/emerald-rogue-assistant` when `XDG_DATA_HOME` is not set
- Linux settings: `$XDG_CONFIG_HOME/emerald-rogue-assistant`, or
  `~/.config/emerald-rogue-assistant` when `XDG_CONFIG_HOME` is not set

The log is `logs/RogueAssistant.log` in the data folder. The Lua script is in
the `scripts` folder. Home Box files are stored by ROM edition and trainer ID.

`settings.ini` can contain these keys:

```ini
Multiplayer.HostPort=30025
Multiplayer.JoinIP=
Bridge.Port=30125
```

On Windows, the first run can copy data from the original assistant. It checks
`%APPDATA%\.pokabbie\rogue_assistant` and a `settings.ini` file in the old app's
working folder. It copies these files only when the new data folder does not
exist. It does not remove the old files.

## Remove the app

Delete the app or the extracted package. User files remain in the folders
listed above. Back up any Home Box files that you want to keep before you
delete those folders.

## Move Home Box data

This app reads and writes the same Home Box file format as the original
Windows assistant. It does not convert your files to a new format.

The apps use separate data folders. The first-run Windows import described
above copies existing files; it does not keep the folders in sync.

To move data between apps or computers:

1. Finish any storage transfer and leave Extended Storage in the game.
2. Close both assistant apps.
3. Back up the game save and both assistant data folders.
4. Copy the latest `<edition>/<trainer ID>/boxes.dat` to the same relative
   path in the destination app's data folder. Keep the edition and trainer
   folders unchanged.

The original Windows app's data folder is
`%APPDATA%\.pokabbie\rogue_assistant`. This app's folders are listed under
[User files](#user-files). Use the matching game save and ROM edition.
