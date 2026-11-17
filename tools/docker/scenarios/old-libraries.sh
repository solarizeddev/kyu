# A program still using the old zlib after the update.
setup() { :; }
drive() {
    kyu_check check-before
    python -c 'import zlib, time; time.sleep(9999)' &
    sleep 1
    kyu_run update
}
