# dscleaner

A simple and efficient command-line tool for removing `.DS_Store` files from your macOS system.

## Features

- **Targeted Cleaning**: Clean the current directory or a specific directory you provide.
- **Flexible Search**: Choose between recursive (default) and non-recursive cleaning.
- **Optimized**: Compiled for a small footprint and stripped of symbols to make it lightweight.
- **Resilient**: Gracefully handles macOS file permission errors by skipping inaccessible directories.

## Prerequisites

- A C++17 compatible compiler (e.g., `clang++` on macOS).
- The `make` build automation tool.

## Building

To build the project, simply run `make` in the root directory:

```bash
make
```

The executable will be created at `bin/dscleaner`.

To recompile from scratch, you can run `make clean && make`.

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

## A Note on macOS Permissions

Modern versions of macOS have a security feature that requires you to grant explicit permission for applications to access protected folders like `~/Desktop`, `~/Documents`, and `~/Downloads`.

If you see an `Operation not permitted` error while scanning, it means `dscleaner` (running via your Terminal) was blocked.

To allow it to scan everywhere, you must grant **Full Disk Access** to your Terminal application:

1. Open **System Settings** > **Privacy & Security**.
2. Click on **Full Disk Access**.
3. Add your Terminal application (e.g., `Terminal.app`, `iTerm.app`) to the list and enable it.
4. Restart your Terminal application.

After this, `dscleaner` will be able to scan any directory you provide.
