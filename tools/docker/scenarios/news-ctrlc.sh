# Ctrl+C at Kyu's own news question: what is left behind, and the next run.
setup() { sed -i 's/^\[2026-1[01]-[0-9]*T/[2026-09-01T/' /var/log/pacman.log; }
drive() {
    kyu_check check-before
    DRIVE_NEWS=ctrlc kyu_run first
    ls -la $state > /out/state-after.txt; cat $state/activity $state/waiting >> /out/state-after.txt 2>&1
    QT_QPA_PLATFORM=offscreen kyu --screenshot /out/card-after.png 2>/dev/null
    DRIVE_NEWS=n kyu_run second
}
