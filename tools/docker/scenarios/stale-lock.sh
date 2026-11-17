# pacman's lock left over (no program has it): Kyu says so, [f] removes it
# and the update runs.
setup() { touch /var/lib/pacman/db.lck; }
export DRIVE_KEYS="f,"
