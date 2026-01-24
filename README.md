# jab

A simple desktop background tool for Wayland. This isn't offering anything more
than the other tools - it was just a place for me to learn Wayland client
development. Specifically with using Pixman and libwayland only.

## Dependencies

* wayland-client
* wayland-protocols
* pixman

No external dependencies are necessary for loading images. This is handled by
[stb_image][stb], which is included in the repository and statically linked at
build time. jab also uses [tllist][tllist] as a submodule. Using the system
installation of tllist isn't handled at this time.

## Building

```
$ make
# make PREFIX=/usr install
```

Currently, only JPEG and PNG images are enabled for stb_image, but this can
easily be changed if desired.

## References

* https://codeberg.org/dnkl/wbg
* https://codeberg.org/sewn/mew
* https://codeberg.org/sewn/drwl
* https://github.com/swaywm/swaybg

[stb]: https://github.com/nothings/stb
[tllist]: https://codeberg.org/dnkl/tllist
