# The news fetch fails: said, and the old news kept (or not): row 12.
setup() { sed -i 's/^\[2026-1[01]-[0-9]*T/[2026-09-01T/' /var/log/pacman.log; }
drive() {
    kyu_check good
    kyu-check --json --news-feed https://forum.manjaro.org/nope.rss > /out/fail.json 2>/out/fail.err; echo "exit $?" >> /out/fail.err
    QT_QPA_PLATFORM=offscreen kyu --screenshot /out/card.png 2>/dev/null
    DRIVE_NEWS=n kyu_run update
}
