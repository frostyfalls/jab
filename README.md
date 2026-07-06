# waywallpaper

A wallpaper for Wayland.

## Dependencies

* wayland
* pixman
* libpng (optional)
* libjpeg-turbo (optional)

## Building

Using a [Meson](https://mesonbuild.com/)-compatible build system, set up the
build directory and configure the list of supported formats.

```
muon setup [-Dpng=disabled] [-Djpeg=disabled] [-Dwebp=disabled] build
samu -C build
```
