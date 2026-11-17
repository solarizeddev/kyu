# Manjaro's news: asked (Enter = no: stopped), then y, then not asked again.
setup() {
    sed -i 's/^\[2026-1[01]-[0-9]*T/[2026-09-01T/' /var/log/pacman.log
}
drive() {
    kyu_check check-before
    DRIVE_NEWS="" kyu_run first
    DRIVE_NEWS=y DRIVE_PROCEED=n kyu_run second
    kyu_run third
}
