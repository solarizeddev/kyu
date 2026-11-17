# kyu-check's failures: no network, a corrupt list, two checks at once.
setup() { :; }
drive() {
    kyu_check good
    printf 'Server = http://127.0.0.1:9/$repo/os/$arch\n' > /tmp/mirrorlist-bad
    echo '{ broken' > $state/updates.json
    kyu-check --json > /out/corrupt.json 2>/out/corrupt.err; echo "exit $?" >> /out/corrupt.err
    (kyu-check --json > /out/twice-1.json 2>/out/twice-1.err &)
    kyu-check --json > /out/twice-2.json 2>/out/twice-2.err; echo "exit $?" >> /out/twice-2.err
    wait
    QT_QPA_PLATFORM=offscreen kyu --screenshot /out/card.png 2>/dev/null
}
after() { :; }
