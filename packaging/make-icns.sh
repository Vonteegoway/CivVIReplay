#!/bin/sh
# Build a macOS .icns from a square PNG using the system tools (sips, iconutil).
# Usage: make-icns.sh <icon.png> <out.icns>
set -e
src="$1"
out="$2"
work="$(mktemp -d)"
set="$work/icon.iconset"
mkdir -p "$set"
for size in 16 32 128 256 512; do
    sips -z $size $size "$src" --out "$set/icon_${size}x${size}.png" >/dev/null
    double=$((size * 2))
    sips -z $double $double "$src" --out "$set/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$set" -o "$out"
rm -rf "$work"
