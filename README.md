# dscleaner

A simple and efficient command-line tool for removing macOS junk files
(`.DS_Store`, `._*` AppleDouble side-cars, `.Spotlight-V100`, `.Trashes`,
`.fseventsd`, and friends) — and for keeping them off your USB drives, archives
and remote servers in the first place.

## Features

- **Comprehensive junk detection**: Removes `.DS_Store`, the `._*` AppleDouble
  files that leak onto USB drives, plus `.Spotlight-V100`, `.Trashes`,
  `.fseventsd`, `.TemporaryItems`, `.DocumentRevisions-V100`, `.apdisk`,
  `.VolumeIcon.icns` and more (files *and* directories).
- **Junk-aware copying**: `cp`, `scp` and `pack` exclude junk *during* the
  operation and verify the destination is clean before reporting completion.
- **Background service**: Register dscleaner as a launchd service that watches
  your volumes (via native **FSEvents**) and auto-removes junk the moment it
  appears.
- **Flexible Search**: Choose between recursive (default) and non-recursive
  cleaning.
- **Optimized**: Compiled for a small footprint and stripped of symbols.
- **Resilient**: Gracefully skips inaccessible directories instead of aborting.

## Prerequisites

- A C++17 compatible compiler (e.g., `clang++` on macOS).
- The `make` build automation tool.

## Installation

### With Homebrew (Recommended)

```bash
# 1. Add the custom tap
brew tap t2o0n321/dscleaner

# 2. Install the formula
brew install dscleaner
```

### Build from Source

```bash
make            # builds ./bin/dscleaner
make clean      # removes build artifacts
```

On macOS the build links `CoreServices` for native FSEvents; on Linux it falls
back to a portable polling watcher.

## Usage

The tool can be run with or without arguments to specify the target directory and cleaning mode.

### Recursive Cleaning (Default)

This will scan the target directory and all of its subdirectories.

```bash
# Clean the current directory and its subfolders
./bin/dscleaner

# Clean a specific directory and its subfolders
./bin/dscleaner /path/to/your/folder
```

### Non-Recursive Cleaning

Use the `-n` or `--no-recursive` flag to clean *only* the specified directory and not its subfolders.

```bash
# Clean ONLY the current directory
./bin/dscleaner -n

# Clean ONLY a specific directory
./bin/dscleaner /path/to/your/folder -n
./bin/dscleaner /path/to/your/folder --no-recursive
```

## Junk-free copying

The real fix for "macOS keeps writing `._` files onto my USB stick" is to never
let the junk reach the destination. These subcommands exclude junk *during* the
operation and report completion only after the destination is verified clean.

### Local copy

```bash
# Copy a folder, skipping junk; the destination is swept clean afterwards.
dscleaner cp ~/project /Volumes/USB
```

### Remote transfer (scp/rsync)

Uses `rsync -e ssh` with excludes when available (so junk never crosses the
wire), falling back to `scp -r` with a warning if `rsync` is not installed.

```bash
dscleaner scp ~/project server:/srv/app
```

### Archiving (tar / zip)

The archive format is chosen by the output extension: `.tar`, `.tar.gz`/`.tgz`,
`.tar.bz2`, `.tar.xz`, `.tar.zst`, `.zip`, `.7z`, `.rar`. Each format uses the
corresponding tool (`tar`, `zip`, `7z`/`7za`/`7zz`/`7zr`, `rar`), which must be
installed; for `.rar` you need WinRAR's `rar` (the free `unrar` cannot create
archives).

```bash
dscleaner pack backup.tar.gz ~/project      # tarball without .DS_Store / ._*
dscleaner pack release.zip ~/project        # zip without macOS junk
dscleaner pack release.7z ~/project         # 7-Zip without macOS junk
dscleaner pack backup.rar ~/project         # RAR without macOS junk
```

## Background service (auto-clean, incl. drag-and-drop)

Watch one or more paths and automatically remove macOS junk as it appears. On
macOS this uses native FSEvents; elsewhere it falls back to polling.

This is what makes **Finder drag-and-drop** work without any command: once the
service watches your volumes, dragging a folder onto a USB drive triggers the
junk (`.DS_Store`, `._*`) to be removed automatically, and macOS shows a
notification reporting how many items were cleaned.

```bash
# Run in the foreground (Ctrl-C to stop)
dscleaner watch /Volumes/USB

# Register as a launchd service that watches all mounted volumes.
# Defaults to /Volumes when no path is given.
dscleaner install-service /Volumes

# Stop and remove the service
dscleaner uninstall-service
```

The service installs a per-user launchd agent at
`~/Library/LaunchAgents/com.t2o0n321.dscleaner.plist` and logs to
`/tmp/com.t2o0n321.dscleaner.{out,err}.log`.

## Short alias: `dc`

Every command is also available under the shorter name **`dc`** (installed
alongside `dscleaner`), so you can type:

```bash
dc /Volumes/USB
dc cp ~/project /Volumes/USB
dc pack release.7z ~/project
dc install-service /Volumes
```

## All commands

```text
dscleaner [path] [-n|--no-recursive]      Clean a directory (default: .)
dscleaner clean [path] [-n]               Explicit clean form
dscleaner cp <src> <dst>                  Copy, excluding junk; sweep dst clean
dscleaner scp <src...> <[user@]host:dst>  Transfer (rsync/scp) without junk
dscleaner pack <archive> <src...>         Archive (tar*/zip) excluding junk
dscleaner watch [path...]                 Watch path(s) and auto-clean junk
dscleaner install-service [path...]       Register background service (launchd)
dscleaner uninstall-service               Remove the background service
dscleaner help | version
```

## Development

The codebase follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html),
enforced by the checked-in `.clang-format` and `.clang-tidy` configurations.

```bash
make format     # auto-format all sources (clang-format, Google style)
make lint       # static analysis (clang-tidy baseline)
```

### Performance & system load

dscleaner is built to stay light even while sweeping large volumes:

- **Idle cost ≈ 0 on macOS.** The watcher uses event-driven FSEvents, so a
  running service consumes no CPU until a file actually changes. (Non-macOS
  builds fall back to lightweight polling.)
- **Single-byte fast reject.** `junk::IsJunk` runs once per filesystem entry;
  because every macOS junk name starts with `.`, ordinary files are rejected in
  one byte comparison. Filenames are compared as `std::string_view`, so the hot
  path performs no heap allocation.
- **Hand-written assembly for the hottest check.** The `._*` AppleDouble prefix
  test is implemented in inline assembly for **Apple Silicon (AArch64)** and
  **Intel (x86-64)**, with a portable C++ fallback for any other target.
- **Streaming, bounded memory.** Cleaning streams the directory tree and retains
  only the junk paths, so memory stays proportional to the junk found rather
  than to the total file count.
- **Batched event handling.** A burst of file events (e.g. one large
  drag-and-drop) is de-duplicated by directory, so each affected folder is swept
  once instead of once per file.

Reference: a 66,600-entry tree (10% junk) is cleaned in ~85 ms; a re-scan of the
already-clean tree takes ~33 ms.

### Layout

| Path                     | Responsibility                                         |
|--------------------------|--------------------------------------------------------|
| `include/junk.hpp`       | Single source of truth for what counts as macOS junk.  |
| `include/proc.hpp`       | Minimal POSIX process runner (no shell injection).     |
| `src/fileManager/`       | Directory scanning and removal helpers.                |
| `src/cleaner/`           | `Cleaner` — removes junk under a path.                  |
| `src/copier/`            | `Copier` — junk-aware `cp` / `scp` / `pack`.            |
| `src/watcher/`           | `Watcher` — FSEvents (macOS) / polling auto-cleaner.    |
| `src/service/`           | `Service` — launchd install / uninstall.               |
| `src/main.cpp`           | CLI dispatch (backwards compatible with v1).           |

## A Note on macOS Permissions

Modern versions of macOS have a security feature that requires you to grant explicit permission for applications to access protected folders like `~/Desktop`, `~/Documents`, and `~/Downloads`.

If you see an `Operation not permitted` error while scanning, it means `dscleaner` (running via your Terminal) was blocked.

To allow it to scan everywhere, you must grant **Full Disk Access** to your Terminal application:

1. Open **System Settings** > **Privacy & Security**.
2. Click on **Full Disk Access**.
3. Add your Terminal application (e.g., `Terminal.app`, `iTerm.app`) to the list and enable it.
4. Restart your Terminal application.

After this, `dscleaner` will be able to scan any directory you provide.
