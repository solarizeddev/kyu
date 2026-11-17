# A downloaded package damaged in the cache (its signature still there).
# (Fable: may be misread as a keyring problem.)
setup() {
    pacman -Syuw --noconfirm
    f=$(ls /var/cache/pacman/pkg/zlib-*.pkg.tar.zst | head -1)
    truncate -s 2000 "$f"
    echo "damaged: $f"
}
