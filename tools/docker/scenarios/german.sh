# German: pacman's messages translated (the Arch image strips them; put
# back), then: decline, and a files conflict.
setup() {
    set -e
    sed -i 's/^NoExtract/#NoExtract/' /etc/pacman.conf
    pacman -Sy --noconfirm glibc pacman sudo
    sed -i 's/^#de_DE.UTF-8/de_DE.UTF-8/' /etc/locale.gen
    locale-gen
    locale -a | grep -q de_DE.utf8
    ls /usr/share/locale/de/LC_MESSAGES/pacman.mo
    d=/var/lib/pacman/local/zlib-$(pacman -Q zlib | cut -d' ' -f2)
    sed -i '\#^usr/include/zlib.h$#d' "$d/files"
}
drive() {
    export LANG=de_DE.UTF-8 LC_ALL=de_DE.UTF-8
    DRIVE_PROCEED=n kyu_run decline
    kyu_run conflict
}
