# The pending zlib damaged in the cache after downloading (its signature
# still there). (Fable: may be misread as a keyring problem.)
setup() {
    pacman -Syuw --noconfirm
    f=/var/cache/pacman/pkg/$(pacman -Sp zlib | sed 's#.*/##')
    truncate -s 2000 "$f"
    echo "damaged: $f"
}
