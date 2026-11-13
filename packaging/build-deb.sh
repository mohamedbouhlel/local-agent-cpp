#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
version=${1:-1.0.0-1}
case "$version" in
    ''|*[!A-Za-z0-9.+:~_-]*)
        printf 'Invalid Debian package version: %s\n' "$version" >&2
        exit 2
        ;;
esac
architecture=$(dpkg --print-architecture)
[ "$architecture" = amd64 ] || {
    printf 'This package recipe targets Debian amd64, found %s.\n' "$architecture" >&2
    exit 2
}

build_dir=${BUILD_DIR:-"$source_dir/build-package"}
output_dir=${OUTPUT_DIR:-"$source_dir/dist"}
cmake -S "$source_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DAGENT_BUILD_TESTS=OFF
cmake --build "$build_dir" --parallel

stage=$(mktemp -d "${TMPDIR:-/tmp}/lofiterm-deb-stage.XXXXXX")
trap 'rm -rf -- "$stage"' EXIT HUP INT TERM
root="$stage/root"
install -D -m 0755 "$build_dir/agent" "$root/usr/lib/lofiterm/agent"
install -D -m 0755 "$script_dir/lofiterm" "$root/usr/bin/lofiterm"
install -D -m 0644 "$script_dir/lofiterm.conf.example" "$root/usr/share/doc/lofiterm/lofiterm.conf.example"
install -D -m 0644 "$source_dir/docs/INSTALL_DEBIAN13.md" "$root/usr/share/doc/lofiterm/INSTALL_DEBIAN13.md"
install -D -m 0644 "$script_dir/copyright" "$root/usr/share/doc/lofiterm/copyright"

mkdir -p -- "$root/DEBIAN"
sed "s/@VERSION@/$version/" "$script_dir/control.in" > "$root/DEBIAN/control"
chmod 0644 "$root/DEBIAN/control"
mkdir -p -- "$output_dir"
output="$output_dir/lofiterm_${version}_${architecture}.deb"
dpkg-deb --build --root-owner-group "$root" "$output"
sha256sum "$output"
printf 'Package: %s\n' "$output"
