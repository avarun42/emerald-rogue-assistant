#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: package.sh BUILD_DIR OUTPUT_DIR VERSION" >&2
  exit 2
fi

build_dir="$1"
output_dir="$2"
version="$3"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
repo_root="$(cd "$script_dir/../.." && pwd -P)"

if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?$ ]]; then
  echo "invalid application version: $version" >&2
  exit 2
fi
if [[ ! -d "$build_dir" ]]; then
  echo "build directory does not exist: $build_dir" >&2
  exit 2
fi
if [[ "$(uname -s)" != Darwin ]]; then
  echo "macOS packaging must run on macOS" >&2
  exit 2
fi

build_dir="$(cd "$build_dir" && pwd -P)"
mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd -P)"

stage_root="$build_dir/macos-package"
case "$stage_root" in
  "$build_dir"/macos-package) ;;
  *)
    echo "refusing unsafe staging path: $stage_root" >&2
    exit 2
    ;;
esac

cmake -E rm -rf "$stage_root"
cmake -E make_directory "$stage_root"
cmake --install "$build_dir" --prefix "$stage_root" --component RogueAssistant --strip
cmake \
  "-DROGUE_INSTALL_ROOT=$stage_root" \
  -DROGUE_INSTALL_PLATFORM=macos \
  "-DROGUE_EXPECTED_VERSION=$version" \
  -P "$repo_root/cmake/VerifyInstall.cmake"

app="$stage_root/RogueAssistant.app"
executable="$app/Contents/MacOS/RogueAssistant"
if [[ ! -x "$executable" ]]; then
  echo "installed application bundle is missing its executable" >&2
  exit 1
fi
if [[ "$(lipo -archs "$executable")" != arm64 ]]; then
  echo "application executable is not arm64-only" >&2
  exit 1
fi

ln -s /Applications "$stage_root/Applications"

signing_identity="${APPLE_SIGNING_IDENTITY:-}"
apple_id="${APPLE_ID:-}"
apple_team_id="${APPLE_TEAM_ID:-}"
apple_app_password="${APPLE_APP_PASSWORD:-}"

notary_value_count=0
for value in "$apple_id" "$apple_team_id" "$apple_app_password"; do
  if [[ -n "$value" ]]; then
    notary_value_count=$((notary_value_count + 1))
  fi
done
if [[ $notary_value_count -ne 0 && $notary_value_count -ne 3 ]]; then
  echo "APPLE_ID, APPLE_TEAM_ID, and APPLE_APP_PASSWORD must be provided together" >&2
  exit 2
fi
if [[ $notary_value_count -eq 3 && -z "$signing_identity" ]]; then
  echo "notarization requires APPLE_SIGNING_IDENTITY" >&2
  exit 2
fi

xattr -cr "$app"
if [[ -n "$signing_identity" ]]; then
  codesign --force --options runtime --timestamp --sign "$signing_identity" "$app"
else
  codesign --force --sign - "$app"
fi
codesign --verify --deep --strict --verbose=2 "$app"

zip_output="$output_dir/RogueAssistant-$version-macos-arm64.zip"
dmg_output="$output_dir/RogueAssistant-$version-macos-arm64.dmg"
notary_zip="$output_dir/.RogueAssistant-$version-notarization.zip"
cmake -E rm -f "$zip_output" "$dmg_output" "$notary_zip"

if [[ $notary_value_count -eq 3 ]]; then
  ditto -c -k --sequesterRsrc --keepParent "$app" "$notary_zip"
  xcrun notarytool submit "$notary_zip" \
    --apple-id "$apple_id" \
    --team-id "$apple_team_id" \
    --password "$apple_app_password" \
    --wait
  xcrun stapler staple "$app"
  cmake -E rm -f "$notary_zip"
fi

ditto -c -k --sequesterRsrc --keepParent "$app" "$zip_output"
hdiutil create \
  -quiet \
  -format UDZO \
  -fs HFS+ \
  -volname "Emerald Rogue Assistant $version" \
  -srcfolder "$stage_root" \
  "$dmg_output"

if [[ -n "$signing_identity" ]]; then
  codesign --force --timestamp --sign "$signing_identity" "$dmg_output"
fi
if [[ $notary_value_count -eq 3 ]]; then
  xcrun notarytool submit "$dmg_output" \
    --apple-id "$apple_id" \
    --team-id "$apple_team_id" \
    --password "$apple_app_password" \
    --wait
  xcrun stapler staple "$dmg_output"
fi

test -s "$zip_output"
test -s "$dmg_output"

hdiutil verify -quiet "$dmg_output"
zip_verify_root="$build_dir/macos-zip-verify"
cmake -E rm -rf "$zip_verify_root"
cmake -E make_directory "$zip_verify_root"
ditto -x -k "$zip_output" "$zip_verify_root"
cmake \
  "-DROGUE_INSTALL_ROOT=$zip_verify_root" \
  -DROGUE_INSTALL_PLATFORM=macos \
  "-DROGUE_EXPECTED_VERSION=$version" \
  -P "$repo_root/cmake/VerifyInstall.cmake"
codesign --verify --deep --strict --verbose=2 "$zip_verify_root/RogueAssistant.app"
if [[ "$(lipo -archs "$zip_verify_root/RogueAssistant.app/Contents/MacOS/RogueAssistant")" != arm64 ]]; then
  echo "archived application executable is not arm64-only" >&2
  exit 1
fi
cmake -E rm -rf "$zip_verify_root"
