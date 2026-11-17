# pacman's keys present but no longer trusted (the keyring is current).
# The fix offers the keyring; after it, Kyu says to populate the keys.
setup() {
    rm -rf /etc/pacman.d/gnupg
    pacman-key --init >/dev/null 2>&1
    pacman-key --add /usr/share/pacman/keyrings/archlinux.gpg >/dev/null 2>&1
}
export DRIVE_KEYS="f,"
