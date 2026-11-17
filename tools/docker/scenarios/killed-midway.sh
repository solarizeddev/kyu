# pacman killed in the middle of the transaction (big set), then a second run.
setup() { :; }
drive() {
    kyu_check check-before
    DRIVE_KILL="upgrading" kyu_run update
    DRIVE_KEYS="f," kyu_run again
}
