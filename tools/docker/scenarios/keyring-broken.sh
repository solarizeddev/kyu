# pacman's keys present but no longer trusted (the keyring is current):
# Kyu says to populate the keys; then that advice, followed, works.
setup() {
    rm -rf /etc/pacman.d/gnupg
    pacman-key --init >/dev/null 2>&1
    pacman-key --add /usr/share/pacman/keyrings/archlinux.gpg >/dev/null 2>&1
}
export DRIVE_KEYS="f,"
after() {
    pacman-key --populate archlinux > /out/populate.log 2>&1
    su tester -c 'export HOME=/home/tester; cd; source /lib.sh; kyu_run after-populate' >> /out/drive.log 2>&1
}
