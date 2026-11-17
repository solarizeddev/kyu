# A real flatpak with a local repository: an update of a tiny runtime,
# alone (no password), then declined, then the repository gone.
setup() {
    mkdir -p /srv/fpsrc/files/share
    printf '[Runtime]\nname=org.kyu.Platform\nruntime=org.kyu.Platform/x86_64/1\nsdk=org.kyu.Platform/x86_64/1\n' > /srv/fpsrc/metadata
    echo one > /srv/fpsrc/files/share/kyu.txt
    flatpak build-export --runtime /srv/fp /srv/fpsrc 1
    chmod -R a+rwX /srv
    su tester -c 'flatpak remote-add --user --no-gpg-verify kyu file:///srv/fp && flatpak install --user -y --noninteractive kyu org.kyu.Platform//1'
    echo two > /srv/fpsrc/files/share/kyu.txt
    flatpak build-export --runtime /srv/fp /srv/fpsrc 1
    chmod -R a+rwX /srv
    pacman -Syu --noconfirm
}
drive() {
    kyu_check check-before
    DRIVE_FLATPAK=n kyu_run declined
    kyu_check check-2
    kyu_run update
    kyu_check check-3
    echo three > /srv/fpsrc/files/share/kyu.txt
    flatpak build-export --runtime /srv/fp /srv/fpsrc 1 >/dev/null
    kyu_check check-4
    mv /srv/fp /srv/fp-gone
    kyu_run gone
}
