# The lock taken after Kyu looked, before pacman runs (at sudo's prompt):
# pacman cannot lock; Kyu waits, finds nobody, says left over; [f] fixes it.
setup() {
    ( for i in $(seq 1 600); do pgrep -x sudo >/dev/null && { touch /var/lib/pacman/db.lck; exit; }; sleep 0.1; done ) &
}
export DRIVE_KEYS="f,"
