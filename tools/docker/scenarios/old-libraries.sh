# A program still using the old zlib after the update.
setup() { :; }
drive() {
    kyu_check check-before
    python -c 'import zlib, time; time.sleep(9999)' &
    py=$!
    sleep 1
    grep libz /proc/$py/maps > /out/maps-before.txt 2>&1
    kyu_run update
    grep libz /proc/$py/maps > /out/maps-after.txt 2>&1
}
