# Neither sudo, doas nor pkexec.
setup() { pacman -Rdd --noconfirm sudo polkit; }
