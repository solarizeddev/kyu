# Neither sudo, doas nor pkexec (polkit's library stays: Flatpak needs it).
setup() { pacman -Rdd --noconfirm sudo; rm -f /usr/bin/pkexec; }
