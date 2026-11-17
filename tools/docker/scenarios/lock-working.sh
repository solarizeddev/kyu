# A real pacman holds the lock, waiting at its own question for 25 s, then
# answered "n": Kyu waits, then goes on.
setup() {
    ( sleep 25; echo n ) | setsid script -q -c 'pacman -S nano' /dev/null >/tmp/other-pacman.log 2>&1 &
    sleep 3
    ls -la /var/lib/pacman/db.lck
}
