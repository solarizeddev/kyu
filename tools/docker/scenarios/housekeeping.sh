# After a whole update: the image's orphans (base and manjaro-release among
# them, never offered: protected), old versions
# and a removed package in the cache (sparse files), shown as "Can be cleaned
# up"; [c] removes the orphan (pacman's own question) and cleans the cache
# (Kyu's [y/N]) with the real paccache.
setup() {
    pacman -Syu --noconfirm
    pacman -D --asdeps nano
    cd /var/cache/pacman/pkg
    for v in 1 2 3 4 5; do truncate -s 400M "zstd-1.5.$v-1-x86_64.pkg.tar.zst"; done
    truncate -s 900M gone-2.0-1-any.pkg.tar.zst
    pacman -Qtdq
}
export DRIVE_KEYS="c," DRIVE_OTHER=y
after() {
    pacman -Q nano && echo "nano still installed" || echo "nano removed"
    ls /var/cache/pacman/pkg
}
