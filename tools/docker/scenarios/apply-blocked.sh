# --apply refuses before anything runs: removing glibc (others need it) and
# a virtual name (java-runtime: pacman would pick a provider silently).
setup() { pacman -Syu --noconfirm; }
drive() {
    cat > ~/q.json <<'JSON'
{"version":1,"items":[{"id":"1","kind":"remove","name":"glibc"},{"id":"2","kind":"install","name":"java-runtime"}]}
JSON
    kyu-update --plan ~/q.json > /out/plan.json 2>/out/plan.err
    kyu_run apply --apply /home/tester/q.json
}
after() { pacman -Q glibc; tail -n 3 /var/log/pacman.log; }
