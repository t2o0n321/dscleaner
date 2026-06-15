# dscleaner — macOS menu bar app

A native SwiftUI **menu bar** front-end for dscleaner. It shows the service
status at a glance, lets you turn automatic drive-cleaning on/off, edit the
watched locations, and run a one-off clean — all without the terminal.

> Requires **macOS 13 (Ventura) or newer** (uses SwiftUI `MenuBarExtra`).

## Architecture

The GUI is a thin shell over the `dscleaner` CLI — it contains **no** cleaning
logic of its own. It runs the binary and renders the result, keeping the C++
core (`../core`) the single source of truth:

| GUI action                         | CLI invoked                       |
|------------------------------------|-----------------------------------|
| Read status (polled every 5 s)     | `dscleaner status --json`         |
| Toggle automatic cleaning on       | `dscleaner install-service <paths>` |
| Toggle automatic cleaning off      | `dscleaner uninstall-service`     |
| Apply watched locations            | `dscleaner install-service <paths>` |
| Clean a folder…                    | `dscleaner clean <path>`          |

```
Sources/DscleanerMenuBar/
  DscleanerApp.swift     @main App: MenuBarExtra + Settings scene, accessory mode
  AppModel.swift         ObservableObject: state, timer refresh, async actions
  CoreClient.swift       runs the dscleaner CLI, decodes `status --json`
  Models.swift           Codable mirror of the status JSON
  MenuContentView.swift  the menu bar panel (status pill + actions)
  SettingsView.swift     settings window (service toggle + watched paths)
  FolderPicker.swift     NSOpenPanel wrapper
```

## Build & run

First build the core so the `dscleaner` binary exists:

```bash
make -C ../core            # produces ../core/bin/dscleaner
```

Then build/run the app (from this directory):

```bash
swift build -c release
swift run DscleanerMenuBar
```

### Finding the CLI

The app looks for `dscleaner` in this order:

1. `$DSCLEANER_BIN` (an explicit path), e.g. for development:
   ```bash
   DSCLEANER_BIN="$PWD/../core/bin/dscleaner" swift run DscleanerMenuBar
   ```
2. a `dscleaner` bundled in the app's `Resources`,
3. `/opt/homebrew/bin`, `/usr/local/bin`, `/usr/bin` (Homebrew / system installs).

## Packaging a distributable `.app`

`swift run` is enough for development. For a signed, double-clickable
`DscleanerMenuBar.app` (with `LSUIElement` so it never shows in the Dock),
wrap this package in an Xcode app target, or bundle the SwiftPM product and add
an `Info.plist` with `LSUIElement = YES`. The CLI can be embedded by copying
`../core/bin/dscleaner` into the app's `Resources` (resolver step 2 above).
