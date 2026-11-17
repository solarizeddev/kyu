# A file the update installs is on disk but no package owns it.
setup() {
    d=$(ls -d /var/lib/pacman/local/zlib-*)
    sed -i '\#^usr/include/zlib.h$#d' "$d/files"
    grep -c zlib.h "$d/files" || true
}
