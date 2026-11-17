# A real flatpak with a local repository: a tiny runtime with an update.
# Alone (no password); declined; updated; then the repository gone.
fp_export() {
    echo "$1" > /srv/fpsrc/usr/share/kyu.txt
    echo "$1" > /srv/fpsrc/files/share/kyu.txt
    flatpak build-export --runtime /srv/fp /srv/fpsrc 1
    chmod -R a+rwX /srv
}
setup() {
    set -e
    pacman -Syu --noconfirm
    mkdir -p /srv/fpsrc/usr/share /srv/fpsrc/files/share
    printf '[Runtime]\nname=org.kyu.Platform\nruntime=org.kyu.Platform/x86_64/1\nsdk=org.kyu.Platform/x86_64/1\n' > /srv/fpsrc/metadata
    fp_export one
    su tester -c 'flatpak remote-add --user --no-gpg-verify kyu file:///srv/fp && flatpak install --user -y --noninteractive kyu org.kyu.Platform//1'
    fp_export two
}
drive() {
    kyu_check check-before
    DRIVE_FLATPAK=n kyu_run declined
    kyu_check check-2
    kyu_run update
    kyu_check check-3
    echo three > /srv/fpsrc/usr/share/kyu.txt; echo three > /srv/fpsrc/files/share/kyu.txt
    flatpak build-export --runtime /srv/fp /srv/fpsrc 1 >/dev/null 2>&1
    kyu_check check-4
    mv /srv/fp /srv/fp-gone
    kyu_run gone
}
