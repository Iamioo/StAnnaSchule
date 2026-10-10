#!/bin/sh
# Builds the Linux version. Needs a C compiler and the X11 headers:
#   Debian/Ubuntu: sudo apt install gcc libx11-dev
#   Fedora:        sudo dnf install gcc libX11-devel
#   Arch:          sudo pacman -S gcc libx11
cd "$(dirname "$0")" || exit 1
mkdir -p bin
${CC:-cc} -O2 -Wall -Wno-pointer-sign -o bin/stanna src/game.c src/plat_x11.c -lX11 && echo "fertig: bin/stanna"
