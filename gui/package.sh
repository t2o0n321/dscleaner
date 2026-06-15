#!/usr/bin/env bash
#
# Builds a distributable DscleanerMenuBar.app:
#   1. builds the C++ core (../core/bin/dscleaner),
#   2. builds the SwiftUI app in release,
#   3. assembles a .app bundle with an Info.plist (LSUIElement = no Dock icon)
#      and the dscleaner CLI embedded in Resources (so the app finds it with no
#      Homebrew install),
#   4. ad-hoc codesigns it (replace with a Developer ID for real distribution).
#
# Run on macOS 13+ with the Swift toolchain:  ./package.sh  [output_dir]
set -euo pipefail

cd "$(dirname "$0")"
GUI_DIR="$PWD"
CORE_DIR="$(cd .. && pwd)/core"
OUT_DIR="${1:-$GUI_DIR/dist}"
APP="$OUT_DIR/DscleanerMenuBar.app"

APP_NAME="DscleanerMenuBar"
BUNDLE_ID="com.t2o0n321.dscleaner.menubar"

echo "==> Building core (CLI)"
make -C "$CORE_DIR"
CLI="$CORE_DIR/bin/dscleaner"
VERSION="$("$CLI" version | awk '{print $2}')"
VERSION="${VERSION:-1.0.0}"

echo "==> Building SwiftUI app (release)"
swift build -c release
EXE="$(swift build -c release --show-bin-path)/$APP_NAME"

echo "==> Assembling $APP"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

cp "$EXE" "$APP/Contents/MacOS/$APP_NAME"
cp "$CLI" "$APP/Contents/Resources/dscleaner"
chmod +x "$APP/Contents/MacOS/$APP_NAME" "$APP/Contents/Resources/dscleaner"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>            <string>${APP_NAME}</string>
  <key>CFBundleDisplayName</key>     <string>dscleaner</string>
  <key>CFBundleIdentifier</key>      <string>${BUNDLE_ID}</string>
  <key>CFBundleExecutable</key>      <string>${APP_NAME}</string>
  <key>CFBundlePackageType</key>     <string>APPL</string>
  <key>CFBundleShortVersionString</key> <string>${VERSION}</string>
  <key>CFBundleVersion</key>         <string>${VERSION}</string>
  <key>LSMinimumSystemVersion</key>  <string>13.0</string>
  <key>LSUIElement</key>             <true/>
  <key>NSHighResolutionCapable</key> <true/>
</dict>
</plist>
PLIST

printf 'APPL????' > "$APP/Contents/PkgInfo"

echo "==> Ad-hoc codesigning (use a Developer ID for distribution)"
codesign --force --deep --sign - "$APP" || echo "   (codesign skipped/failed; app still runnable locally)"

echo "==> Done: $APP"
echo "    Launch with: open \"$APP\""
