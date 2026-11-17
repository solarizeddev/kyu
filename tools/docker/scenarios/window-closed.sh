# The terminal closed while pacman upgrades (big set): what is left, and the next run.
setup() { :; }
drive() {
    kyu_check check-before
    DRIVE_HANGUP="upgrading" kyu_run update
    sleep 20
    ls -la $state > /out/state-after-close.txt 2>&1; cat $state/activity $state/waiting >> /out/state-after-close.txt 2>&1
    QT_QPA_PLATFORM=offscreen kyu --screenshot /out/card-after-close.png 2>/dev/null
    DRIVE_KEYS="f," kyu_run again
}
