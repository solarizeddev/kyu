# No mirror answers: the check fails, the update fails before changing anything.
setup() { printf 'Server = http://127.0.0.1:9/$repo/os/$arch\n' > /etc/pacman.d/mirrorlist; }
