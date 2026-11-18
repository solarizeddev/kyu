# --apply on a system synced but not upgraded (the image is older than the
# mirror; pamac's background refresh does this): the planner must say
# "update first", and tree joins the update's own pacman -Syu --needed.
setup() { pacman -Sy; pacman -Qu | wc -l; }
export DRIVE_OTHER=y DRIVE_KEYS=","
drive() {
    cat > ~/q.json <<'JSON'
{"version":1,"items":[{"id":"1","kind":"install","name":"tree"}]}
JSON
    kyu-update --plan ~/q.json > /out/plan.json 2>/out/plan.err
    kyu_run apply --apply /home/tester/q.json
}
after() { pacman -Q tree; pacman -Qu | wc -l; grep -a "Running 'pacman" /var/log/pacman.log | tail -3; }
