libportal - Flatpak portal library
==================================

libportal provides GIO-style async APIs for most Flatpak portals.


## Getting started

### The Meson build-system

`libportal` uses the Meson to build its projects. To install Meson on your system, follow instructions here: https://mesonbuild.com/Getting-meson.html. If your system is missing a dependency, Meson will tell you which one. It is up to you, how you install your system dependencies.

### Build libportal
```
meson build/libportal
ninja -C build/libportal
```

### Build and run portal-test
```
meson build/potal-test -Dbuild-portal-test=true
ninja -C build/portal-test
chmod +x ./build/potal-test/portal-test/portal-test
./build/potal-test/portal-test/portal-test
```

### Build and view docs
```
meson build/doc -Dgtk_doc=true
ninja -C build/doc

# @TODO How to view the docs?
```
