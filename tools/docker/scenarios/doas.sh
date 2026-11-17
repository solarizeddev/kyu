# doas instead of sudo.
setup() {
    pacman -S --noconfirm opendoas
    echo 'permit persist tester' > /etc/doas.conf
    pacman -Rdd --noconfirm sudo
}
export DRIVE_PASSWORD=kyu
