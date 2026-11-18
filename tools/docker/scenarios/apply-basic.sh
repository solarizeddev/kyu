# --apply with the real pacman on a current system: install tree, install
# which as a dependency, remove nano (installed here first), in one queue:
# one -S --needed, then -Rs, then -D --asdeps; each pacman's own question.
setup() {
    pacman -Syu --noconfirm
    pacman -S --noconfirm --needed nano
}
export DRIVE_KEYS=","
drive() {
    cat > ~/q.json <<'JSON'
{"version":1,"items":[{"id":"1","kind":"remove","name":"nano"},{"id":"2","kind":"install","name":"tree"},{"id":"3","kind":"install","name":"which","asDependency":true}]}
JSON
    kyu-update --plan ~/q.json > /out/plan.json 2>/out/plan.err
    kyu_run apply --apply /home/tester/q.json
    cp "$state/apply-result.json" /out/ 2>/dev/null || true
}
after() {
    pacman -Q nano 2>&1; pacman -Q tree which 2>&1
    pacman -Qi which | grep -i reason
}
