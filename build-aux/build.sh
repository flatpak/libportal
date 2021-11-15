#!/bin/sh

set -eu

TOP_DIR=`dirname $0`/..
OLD_DIR=`pwd`

cd "$TOP_DIR"

for backend in Gtk3 Gtk4 Qt5; do
    flatpak-builder --force-clean --ccache --repo=_build/repo --install --user "_build/app-$backend" "build-aux/org.gnome.PortalTest.${backend}.json"
done

cd "$OLD_DIR"
