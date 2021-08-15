#!/bin/sh

set -eu

TOP_DIR=`dirname $0`/..
OLD_DIR=`pwd`

cd "$TOP_DIR"
JSON=build-aux/org.gnome.PortalTest.Gtk3.json
# Keep in sync with manifest
MESON_ARGS="-Dportal-tests=gtk3 -Dgtk_doc=false"

rm -rf _flatpak_meson_build
flatpak-builder --force-clean --ccache --repo=repo --install --user --stop-at=portal-test-gtk3 app $JSON
flatpak build app meson --prefix=/app --libdir=lib $MESON_ARGS _flatpak_meson_build
flatpak build app ninja -C _flatpak_meson_build install
flatpak-builder --finish-only --repo=repo app $JSON
cd "$OLD_DIR"
