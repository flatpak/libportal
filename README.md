libportal - Flatpak portal library
==================================

libportal provides GIO-style async APIs for most Flatpak portals.

## Getting started

### The Meson build system

`libportal` uses Meson to build its projects. To install Meson on your system, follow the [Getting Meson instructions](https://mesonbuild.com/Getting-meson.html). If your system is missing a dependency, Meson will tell you which one. How you install your missing dependencies is up to you.

### Build libportal

The first time you build libportal, give `meson` a directory to build into; we recommend `_build`:

```
meson _build
```

Then use `ninja` to build the project, pointing it to your build directory:

```
ninja -C _build
```

For subsequent builds, you only need to use the `ninja -C _build` command.

### Build and run portal-test

If this is your first time building libportal, configure the project to include portal-test:

```
meson _build -Dbuild-portal-test=true
```

Otherwise, re-configure the project:

```
meson configure _build -Dbuild-portal-test=true
```

Then build:

```
ninja -C _build
```

And run the binary:

```
./_build/portal-test/portal-test
```
