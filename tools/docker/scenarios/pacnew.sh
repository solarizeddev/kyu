# nano's /etc/nanorc changed by hand and in the new package: a .pacnew;
# [m] runs pacdiff (no vim here: Kyu picks a viewer), which removes the new
# version ("r").
setup() {
    echo "# changed by hand" >> /etc/nanorc
    d=/var/lib/pacman/local/nano-$(pacman -Q nano | cut -d' ' -f2)
    sed -i 's#^etc/nanorc\t.*#etc/nanorc\t00000000000000000000000000000000#' "$d/files"
    grep -A2 BACKUP "$d/files"
}
export DRIVE_KEYS="m," DRIVE_PACDIFF=r
