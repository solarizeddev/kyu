# Another pacman holds the lock for 30 s, then exits and removes it (as
# pacman does): Kyu waits, then goes on.
setup() {
    touch /var/lib/pacman/db.lck
    cp /usr/bin/bash /usr/local/bin/pacman-stand-in
    ln -sf /usr/local/bin/pacman-stand-in /tmp/pacman
    # A process named "pacman" with the lock open.
    /tmp/pacman -c 'exec 3</var/lib/pacman/db.lck; sleep 30; rm -f /var/lib/pacman/db.lck' &
    sleep 1
}
