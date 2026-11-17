# German: pacman's messages translated (the Arch image strips them; put
# back), then: decline, no connection, a files conflict.
setup() {
    sed -i 's/^NoExtract/#NoExtract/' /etc/pacman.conf
    pacman -S --noconfirm glibc pacman sudo
    sed -i 's/^#de_DE.UTF-8/de_DE.UTF-8/' /etc/locale.gen
    locale-gen
    d=$(ls -d /var/lib/pacman/local/zlib-*)
    sed -i '\#^usr/include/zlib.h$#d' "$d/files"
}
drive() {
    export LANG=de_DE.UTF-8 LC_ALL=de_DE.UTF-8
    DRIVE_PROCEED=n kyu_run decline
    kyu_run conflict
}
