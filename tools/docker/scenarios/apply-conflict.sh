# --apply installing vim while gvim (which conflicts with it) is installed:
# what the dry run makes of a conflict, and pacman's own question removing
# gvim in the same transaction.
setup() {
    pacman -Syu --noconfirm
    pacman -S --noconfirm --needed gvim
}
export DRIVE_OTHER=y DRIVE_KEYS=","
drive() {
    cat > ~/q.json <<'JSON'
{"version":1,"items":[{"id":"1","kind":"install","name":"vim"}]}
JSON
    kyu-update --plan ~/q.json > /out/plan.json 2>/out/plan.err
    kyu_run apply --apply /home/tester/q.json
}
after() { pacman -Q vim gvim 2>&1; }
