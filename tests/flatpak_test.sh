#!/bin/sh
# kyu-flatpak against real libflatpak: a local repository and a throwaway
# user installation built with flatpak's own tools. With --after, also
# against kyu-helper on a private session bus with the fake pacman.
# Run under dbus-run-session (ctest does this):
#   dbus-run-session -- sh tests/flatpak_test.sh BUILD_BIN_DIR
set -eu

bin=$1
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
helper_pid=
trap 'cleanup' EXIT
cleanup() {
    [ -n "$helper_pid" ] && kill -9 "$helper_pid" 2>/dev/null || true
    rm -rf "$work"
}
fail() {
    echo "FAIL: $*" >&2
    for f in "$work"/*.out "$work"/state/kyu/flatpak/*.jsonl; do
        [ -f "$f" ] && { echo "--- $f" >&2; cat "$f" >&2; }
    done
    exit 1
}

# FLATPAK_CONFIG_DIR: no extra system installations from /etc/flatpak.
mkdir -p "$work/config"
export FLATPAK_USER_DIR="$work/user" FLATPAK_SYSTEM_DIR="$work/system" FLATPAK_CONFIG_DIR="$work/config" XDG_STATE_HOME="$work/state"
. "$here/flatpak/make-repo.sh"
make_repo "$work"
flatpak --user list --columns=application | grep -q org.kyu.Test.App || fail "test installation not set up"

last_log() { ls "$work"/state/kyu/flatpak/*.jsonl | tail -n 1; }
has() { grep -qF -- "$2" "$1" || fail "$(basename "$1"): missing $2"; }
seq_of() { grep -F -- "$2" "$1" | head -n 1 | sed -n 's/.*"seq":\([0-9]*\).*/\1/p'; }
run_flatpak() { # NAME EXPECTED_CODE ARGS...
    name=$1; want=$2; shift 2
    set +e
    timeout 120 "$bin/kyu-flatpak" --no-check "$@" >"$work/$name.out" 2>&1
    got=$?
    set -e
    [ "$got" = "$want" ] || fail "$name: exit code $got, expected $want"
}

echo "updates every Flatpak with an update, runtime included"
publish "$work" org.kyu.Test.App 2
publish "$work" "$KYU_TEST_RUNTIME" 2
[ "$(flatpak --user remote-ls --updates --columns=application | wc -l)" = 2 ] || fail "expected 2 pending updates"
run_flatpak update 0
log=$(last_log)
has "$log" '"kind":"started"'
has "$log" '"source":"flatpak"'
has "$log" '"package":"org.kyu.Test.App"'
has "$log" '"package":"org.kyu.Test.Runtime"'
has "$log" '"summary":"2 Flatpaks updated"'
[ -z "$(flatpak --user remote-ls --updates)" ] || fail "updates left after kyu-flatpak"
[ "$(stat -c %a "$log")" = 644 ] || true

echo "nothing to do"
run_flatpak nothing 0
has "$(last_log)" '"summary":"No Flatpak updates"'

echo "a broken remote is reported, the run fails"
publish "$work" org.kyu.Test.Other 2
mv "$work/repo" "$work/repo.away"
run_flatpak broken 1
has "$(last_log)" '"success":false'
has "$(last_log)" 'could not read a remote'
has "$(last_log)" 'Treating remote fetch error as non-fatal'
echo "the checker reports the broken remote too, instead of 'no updates'"
set +e; "$bin/kyu-check" --no-pacman --json >"$work/check.out" 2>&1; code=$?; set -e
[ "$code" = 2 ] || fail "kyu-check exit code $code with a broken remote, expected 2"
grep -A4 '"flatpak": {' "$work/check.out" | grep -q '"ok": false' || fail "kyu-check did not report the broken remote"
has "$work/check.out" "No such ref"
mv "$work/repo.away" "$work/repo"

echo "only one kyu-flatpak at a time"
"$bin/kyu-helper" --session --pacman "$here/helper/fake-pacman" --db-lock "$work/db.lck" \
    --log-dir "$work/helper-logs" 2>/dev/null &
helper_pid=$!
i=0; until "$bin/kyu-update" --session --state >/dev/null 2>&1; do i=$((i + 1)); [ $i -lt 50 ] || fail "helper not up"; sleep 0.1; done
cp "$here/helper/fake-pacman" "$work/pacman"
kill -9 "$helper_pid"; wait "$helper_pid" 2>/dev/null || true
"$bin/kyu-helper" --session --pacman "$work/pacman" --db-lock "$work/db.lck" --log-dir "$work/helper-logs" 2>/dev/null &
helper_pid=$!
i=0; until "$bin/kyu-update" --session --state >/dev/null 2>&1; do i=$((i + 1)); [ $i -lt 50 ] || fail "helper not up"; sleep 0.1; done
echo normal >"$work/mode"; echo 1 >"$work/delay"

"$bin/kyu-update" --session </dev/null >"$work/pacman.out" 2>&1 &
pacman_ui=$!
i=0; until tx=$("$bin/kyu-update" --session --state | sed -n 's/.*"tx": "\([^"]*\)".*/\1/p' | head -n 1); [ -n "$tx" ]; do
    i=$((i + 1)); [ $i -lt 50 ] || fail "no pacman transaction"; sleep 0.1
done
"$bin/kyu-flatpak" --no-check --session-bus --after "$tx" >"$work/after.out" 2>&1 &
after=$!
sleep 0.5
run_flatpak second 3
has "$work/second.out" 'already running'

echo "--after waits for pacman, then updates"
wait "$after" || fail "kyu-flatpak --after failed"
wait "$pacman_ui" || fail "pacman update failed"
log=$(last_log)
has "$log" '"stage":"waiting"'
has "$log" 'pacman finished: 3 upgraded'
has "$log" '"summary":"1 Flatpak updated"'
[ "$(seq_of "$log" 'pacman finished')" -lt "$(seq_of "$log" '"kind":"progress"')" ] || fail "Flatpaks updated before pacman finished"

echo "--after skips Flatpaks when the pacman update is cancelled at a question"
publish "$work" org.kyu.Test.App 3
echo conflict >"$work/mode"
"$bin/kyu-update" --session </dev/null >"$work/cancel.out" 2>&1 &
pacman_ui=$!
i=0; until grep -q 'kyu: type' "$work/cancel.out"; do i=$((i + 1)); [ $i -lt 100 ] || fail "no question"; sleep 0.1; done
tx=$("$bin/kyu-update" --session --state | sed -n 's/.*"tx": "\([^"]*\)".*/\1/p' | head -n 1)
"$bin/kyu-flatpak" --no-check --session-bus --after "$tx" >"$work/skipped.out" 2>&1 &
after=$!
sleep 0.5
gdbus call --session --dest io.github.solarizeddev.kyu.Helper --object-path /io/github/solarizeddev/kyu/Helper \
    --method io.github.solarizeddev.kyu.Helper1.Cancel "'$tx'" >/dev/null
set +e; wait "$after"; code=$?; set -e
[ "$code" = 2 ] || fail "kyu-flatpak exit code $code after a cancelled pacman update, expected 2"
wait "$pacman_ui" 2>/dev/null || true
has "$(last_log)" '"skipped":true'
flatpak --user remote-ls --updates --columns=application | grep -q org.kyu.Test.App || fail "Flatpak updated although pacman was cancelled"

echo "all flatpak tests passed"
