# The window closed while Kyu waits for another pacman: nothing is started.
setup() {
    touch /var/lib/pacman/db.lck
    cp /usr/bin/bash /usr/local/bin/pacman-stand-in; ln -sf /usr/local/bin/pacman-stand-in /tmp/pacman
    /tmp/pacman -c 'exec 3</var/lib/pacman/db.lck; sleep 40; rm -f /var/lib/pacman/db.lck' &
    sleep 1
}
drive() {
    DRIVE_HANGUP="waited 0:0" kyu_run update
    sleep 50
    pgrep -a pacman > /out/pacman-after.txt 2>&1 || echo "no pacman ran" > /out/pacman-after.txt
    ls -la $state > /out/state-after.txt
}
after() { grep -c "starting full system upgrade" /var/log/pacman.log > /out/upgrades-started.txt; }
