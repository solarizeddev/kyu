# Sourced in the container, as "tester", before a scenario's drive().
export PATH=/opt/kyu-build/bin:$PATH
state=$HOME/.local/state/kyu

# kyu-check, its JSON and messages kept (after waiting for one kyu-update started).
kyu_check() {
    local name=$1 i
    for i in $(seq 1 120); do
        kyu-check --json > /out/$name.json 2>/out/$name.err && break
        grep -q "already running" /out/$name.err || break
        sleep 5
    done
}

# kyu-update in a terminal, answered as DRIVE_* say; its record kept.
kyu_run() {
    local name=$1; shift
    python /drive.py /out/$name -- kyu-update "$@"
    cp "$state/last-run.json" /out/$name/last-run.json 2>/dev/null || true
}

# The default: check, then update.
drive() {
    kyu_check check-before
    kyu_run update
    kyu_check check-after
}
