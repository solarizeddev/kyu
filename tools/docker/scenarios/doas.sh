# doas instead of sudo (keepenv: the proxy, as sudo's env_keep).
setup() {
    pacman -Sy --noconfirm opendoas
    echo 'permit persist keepenv tester' > /etc/doas.conf
    pacman -Rdd --noconfirm sudo
}
