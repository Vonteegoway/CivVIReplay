#!/bin/sh
# Build a macOS .icns from a square PNG using the system tools (sips, iconutil).
# Usage: make-icns.sh <icon.png> <out.icns>
set -e
src="$1"
out="$2"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
iconset="$work/icon.iconset"
mkdir -p "$iconset"
for size in 16 32 128 256 512; do
    sips -z $size $size "$src" --out "$iconset/icon_${size}x${size}.png" >/dev/null
    double=$((size * 2))
    sips -z $double $double "$src" --out "$iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$iconset" -o "$out"
