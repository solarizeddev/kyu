# A file the update installs is on disk but no package owns it.
setup() {
    d=/var/lib/pacman/local/zlib-$(pacman -Q zlib | cut -d' ' -f2)
    sed -i '\#^usr/include/zlib.h$#d' "$d/files"
    grep -c zlib.h "$d/files" || echo "zlib.h no longer owned"
}
