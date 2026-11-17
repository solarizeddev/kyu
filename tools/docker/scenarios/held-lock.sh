# Another pacman holds the lock for 40 s: Kyu waits, then goes on.
setup() {
    touch /var/lib/pacman/db.lck
    cp /usr/bin/sleep /usr/local/bin/pacman-holder
    ln -sf /usr/local/bin/pacman-holder /tmp/pacman
    # A process named "pacman" with the lock open, gone after 40 s.
    (exec 3</var/lib/pacman/db.lck; exec -a pacman /tmp/pacman 40; ) &
    ( sleep 41; rm -f /var/lib/pacman/db.lck ) &
    sleep 1
}
