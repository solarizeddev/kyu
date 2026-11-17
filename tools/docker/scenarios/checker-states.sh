# kyu-check's failures: a corrupt list, two checks at once, no network, and
# a lock left in its own copy of the databases.
setup() { :; }
drive() {
    kyu_check good
    echo '{ broken' > $state/updates.json
    kyu-check --json > /out/corrupt.json 2>/out/corrupt.err; echo "exit $?" >> /out/corrupt.err
    (kyu-check --json > /out/twice-1.json 2>/out/twice-1.err &)
    kyu-check --json > /out/twice-2.json 2>/out/twice-2.err; echo "exit $?" >> /out/twice-2.err
    sleep 30
}
after() {
    cp /etc/pacman.d/mirrorlist /tmp/mirrorlist.good
    printf 'Server = http://127.0.0.1:9/$repo/os/$arch\n' > /etc/pacman.d/mirrorlist
    su tester -c 'export PATH=/opt/kyu-build/bin:$PATH; kyu-check --json > /out/nonet.json 2>/out/nonet.err; echo "exit $?" >> /out/nonet.err; QT_QPA_PLATFORM=offscreen kyu --screenshot /out/card-nonet.png' 2>/dev/null
    cp /tmp/mirrorlist.good /etc/pacman.d/mirrorlist
    lockdir=$(find /home/tester/.cache -type d -name 'checkup-db*' | head -1)
    echo "checkup db: $lockdir" > /out/stale-db-lock.err
    [ -n "$lockdir" ] && touch "$lockdir/db.lck"
    su tester -c 'export PATH=/opt/kyu-build/bin:$PATH; kyu-check --json > /out/stale-db-lock.json 2>>/out/stale-db-lock.err; echo "exit $?" >> /out/stale-db-lock.err'
}
