#!/bin/sh

flatpak-builder --force-clean --ccache --repo=repo --install --user app org.gnome.PortalTest.json
