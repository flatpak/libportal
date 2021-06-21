libportal - Flatpak portal library
==================================

libportal provides GIO-style async APIs for most Flatpak portals.

## Getting started

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

### Passing options

libportal includes [Meson build options](https://mesonbuild.com/Build-options.html#build-options) for components that can optionally be built. After first running `meson _build`, you can view the available options with:

```
meson configure _build
```

To change an option, re-configure the project:

```
meson configure _build -Dbuild-portal-test=true
```

You can also pass in options right from the start, e.g. with:

```
meson _build -Dbuild-portal-test=true
```

Then build:

```
ninja -C _build
```

## Optional components

### `portal-test` and `portal-test-qt`

Set the `build-portal-test` or `build-portal-test-qt` option to `true` and build the project to compile the corresponding binary. Then run the binary, e.g.:

```
./_build/portal-test/portal-test
```

or

```
./_build/portal-test/portal-test-qt
```
