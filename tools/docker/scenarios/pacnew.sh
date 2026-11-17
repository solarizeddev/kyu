# nano's /etc/nanorc changed by hand and in the new package: a .pacnew;
# [m] runs pacdiff, which removes the new version ("r").
setup() {
    echo "# changed by hand" >> /etc/nanorc
    d=$(ls -d /var/lib/pacman/local/nano-*)
    sed -i 's#^etc/nanorc\t.*#etc/nanorc\t00000000000000000000000000000000#' "$d/files"
    grep -A2 BACKUP "$d/files"
}
export DRIVE_KEYS="m," DRIVE_PACDIFF=r
