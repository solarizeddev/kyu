# A pamac-daemon that has the lock open but does nothing: called idle;
# [f] refuses to remove a lock that is in use.
setup() {
    cp /usr/bin/bash /usr/local/bin/pamac-daemon
    /usr/local/bin/pamac-daemon -c 'exec 9>/var/lib/pacman/db.lck; kill -STOP $$' &
    sleep 1
}
drive() { kyu_check check-before; DRIVE_KEYS="f," kyu_run update --idle-after 5; }
