# waywallpaper

A wallpaper for Wayland compositors supporting wlr-layer-shell-v1. Written with
minimal dependencies in mind.

## Dependencies

* wayland
* pixman
* libpng (optional, for PNG support)
* libjpeg (optional, for JPEG support)
* libwebp (optional, for WEBP support)

If no image formats are supported, only solid colors are available.

## Building

Using a [Meson](https://mesonbuild.com/)-compatible build system, set up the
build directory and configure the list of supported formats.

```
muon setup [-Dpng=disabled] [-Djpeg=disabled] [-Dwebp=disabled] build
samu -C build
```

## License

waywallpaper is under the MIT license; see LICENSE for more information.
