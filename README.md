libportal - Flatpak portal library
==================================

libportal provides GIO-style async APIs for most Flatpak portals. You can find
the documentation [here](https://flatpak.github.io/libportal/).

## Getting started

`libportal` uses Meson to build its projects. To install Meson on your system,
follow the [Getting Meson instructions][1]. If your system is missing a
dependency, Meson will tell you which one. How you install your missing
dependencies is up to you.

### Build libportal

The first time you build libportal, give `meson` a directory to build into; we
recommend `_build`:

```
meson _build
```

Then use `ninja` to build the project, pointing it to your build directory:

```
ninja -C _build
```

For subsequent builds, you only need to use the `ninja -C _build` command.

### Passing options

libportal includes [Meson build options][2] for components that can optionally
be built. After first running `meson _build`, you can view the available options
with:

```
meson configure _build
```

To change an option, re-configure the project:

```
meson configure _build -Dportal-tests=gtk3
```

You can also pass in options right from the start, e.g. with:

```
meson _build -Dportal-tests=gtk3
```

Then build:

```
ninja -C _build
```

## Optional components

### `portal-tests`

To generate test binaries, set the `portal-tests` option to any combination of
`gtk3`, `gtk4`, or `qt` e.g. `-Dportal-tests=gtk3,gtk4,qt`. Then run the desired 
test binary, e.g.:

```
./_build/portal-test/portal-test/gtk3/portal-test-gtk3
```

or

```
./_build/portal-test/portal-test/qt/portal-test-qt
```

[1]: https://mesonbuild.com/Getting-meson.html
[2]: https://mesonbuild.com/Build-options.html#build-options
