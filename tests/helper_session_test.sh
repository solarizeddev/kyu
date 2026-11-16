#!/bin/sh
# End-to-end test of kyu-helper and kyu-update on a private session bus,
# with a fake pacman. Run under dbus-run-session (ctest does this):
#   dbus-run-session -- sh tests/helper_session_test.sh BUILD_BIN_DIR
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
    for f in "$work"/*.out "$work"/helper.err; do
        [ -f "$f" ] && { echo "--- $f" >&2; cat "$f" >&2; }
    done
    exit 1
}

cp "$here/helper/fake-pacman" "$work/pacman"
mode() { echo "$1" > "$work/mode"; }
delay() { echo "$1" > "$work/delay"; }

# Restart detection looks at a fake /proc and /usr/lib/modules: the running
# kernel 6.18.20 was replaced by 6.18.22, and plasmashell maps a replaced library.
mkdir -p "$work/proc/sys/kernel" "$work/proc/42" "$work/modules/6.18.22-1-MANJARO"
echo 6.18.20-1-MANJARO >"$work/proc/sys/kernel/osrelease"
echo "BOOT_IMAGE=/boot/vmlinuz-linux618 root=UUID=x rw" >"$work/proc/cmdline"
echo plasmashell >"$work/proc/42/comm"
echo "7f00-7f01 r--p 0 00:1f 9  /usr/lib/libQt6Core.so.6 (deleted)" >"$work/proc/42/maps"
touch "$work/modules/6.18.22-1-MANJARO/vmlinuz"
echo linux618 >"$work/modules/6.18.22-1-MANJARO/pkgbase"

start_helper() {
    "$bin/kyu-helper" --session --pacman "$work/pacman" --db-lock "$work/db.lck" \
        --log-dir "$work/logs" --idle-exit 600 --proc-dir "$work/proc" --modules-dir "$work/modules" \
        "$@" 2>>"$work/helper.err" &
    helper_pid=$!
    i=0
    until "$bin/kyu-update" --session --state >/dev/null 2>&1; do
        i=$((i + 1)); [ $i -lt 50 ] || fail "helper did not come up"; sleep 0.1
    done
}

# update NAME [ARGS...]: runs kyu-update with $INPUT on stdin (empty: none),
# stores output in NAME.out, returns its exit code
update() {
    name=$1; shift
    set +e
    printf "${INPUT:-}" | timeout 30 "$bin/kyu-update" --session "$@" >"$work/$name.out" 2>&1
    code=$?
    set -e
    return $code
}

expect_code() {
    want=$1; name=$2; shift 2
    if update "$name" "$@"; then got=0; else got=$?; fi
    [ "$got" = "$want" ] || fail "$name: exit code $got, expected $want"
}

absent() { grep -qF -- "$2" "$work/$1.out" && fail "$1: unexpected '$2'"; true; }

current_tx() {
    "$bin/kyu-update" --session --state | sed -n 's/.*"tx": "\([^"]*\)".*/\1/p' | head -n 1
}

helper_call() {
    method=$1; shift
    gdbus call --session --dest io.github.solarizeddev.kyu.Helper \
        --object-path /io/github/solarizeddev/kyu/Helper \
        --method "io.github.solarizeddev.kyu.Helper1.$method" "$@" 2>&1
}

contains() { grep -qF -- "$2" "$work/$1.out" || fail "$1: missing '$2'"; }
count() { n=$(grep -cF -- "$2" "$work/$1.out" || true); [ "$n" = "$3" ] || fail "$1: '$2' appears $n times, expected $3"; }

wait_for() {
    file=$1; text=$2; i=0
    until grep -qF -- "$text" "$file" 2>/dev/null; do
        i=$((i + 1)); [ $i -lt 100 ] || fail "timed out waiting for '$text' in $file"; sleep 0.1
    done
}

start_helper

echo "interface matches data/io.github.solarizeddev.kyu.Helper1.xml"
gdbus introspect --session --dest io.github.solarizeddev.kyu.Helper \
    --object-path /io/github/solarizeddev/kyu/Helper --xml >"$work/introspect.xml"
for item in 'method name="StartUpdate"' 'method name="GetState"' 'signal name="Event"' \
            'method name="Answer"' 'method name="Cancel"' 'name="question" type="t"' 'name="answer" type="s"' \
            'name="transaction" type="s"' 'name="seq" type="t"' 'name="record" type="s"'; do
    grep -qF "$item" "$work/introspect.xml" || fail "introspection lacks $item"
    grep -qF "$item" "$here/../data/io.github.solarizeddev.kyu.Helper1.xml" || fail "XML file lacks $item"
done

echo "normal update"
mode normal; delay 0
expect_code 0 normal
contains normal 'kyu: running '
contains normal 'pacman -Syu --noprogressbar --color never'
contains normal 'kyu: answered ":: Proceed with installation? [Y/n]" with y'
contains normal 'upgrading mesa...'
contains normal 'kyu: 3 upgraded'
count normal 'upgrading kwin...' 1

echo "after the update: restart facts and settings files"
contains normal 'kyu: restart required: running kernel 6.18.20-1-MANJARO, installed 6.18.22-1-MANJARO'
contains normal 'kyu: 1 program uses replaced libraries: plasmashell'
contains normal 'kyu: /etc/kyu-test.conf.pacnew has new default settings for /etc/kyu-test.conf'
contains normal 'kyu: to compare and merge: sudo pacdiff'
absent normal 'systemd (PID 1)'
tail -n 1 "$(ls "$work"/logs/*.jsonl | tail -n 1)" | grep -q '"required":true' || fail "finished record lacks restart.required"

echo "log file is world-readable JSON lines ending in 'finished'"
log=$(ls "$work"/logs/*.jsonl | tail -n 1)
[ "$(stat -c %a "$log")" = 644 ] || fail "log mode is $(stat -c %a "$log")"
tail -n 1 "$log" | grep -q '"kind":"finished"' || fail "log does not end with finished"
tail -n 1 "$log" | grep -q '"success":true' || fail "log does not record success"

echo "killing the UI mid-update does not stop the update; --attach picks it up"
delay 1
"$bin/kyu-update" --session >"$work/killed.out" 2>&1 &
ui_pid=$!
wait_for "$work/killed.out" 'upgrading kwin...'
kill -9 "$ui_pid"
wait "$ui_pid" 2>/dev/null || true
grep -qF 'kyu: 3 upgraded' "$work/killed.out" && fail "update finished before the UI was killed; test proves nothing"
expect_code 0 attached --attach
count attached 'upgrading kwin...' 1
count attached 'upgrading linux618...' 1
count attached 'upgrading mesa...' 1
contains attached 'kyu: 3 upgraded'

echo "--quiet shows progress"
delay 0
expect_code 0 quiet --quiet
contains quiet '[1/3] upgrading kwin'
contains quiet '[3/3] upgrading mesa'

echo "second start while running is refused"
delay 1
"$bin/kyu-update" --session >"$work/first.out" 2>&1 &
first=$!
wait_for "$work/first.out" 'kyu: running'
expect_code 3 second
contains second 'already running'
wait "$first" || fail "first update failed"
grep -qF 'kyu: 3 upgraded' "$work/first.out" || fail "first update did not finish"

echo "pacman's lock file is respected"
delay 0
touch "$work/db.lck"
expect_code 3 locked
contains locked 'Locked'
contains locked 'db.lck exists'
rm "$work/db.lck"

echo "an unrecognised question stops pacman before it changes anything"
mode unknown
expect_code 1 unknown
contains unknown 'which Kyu does not recognise; stopping it'
contains unknown 'Stopped at a question Kyu does not recognise: ":: Something Kyu has never seen? [Y/n]". No packages were changed.'
absent unknown "instead of stopping pacman"

echo "provider question: pacman's list is shown, the chosen number is passed on"
mode provider
INPUT='3\n' expect_code 0 provider
contains provider ':: There are 3 providers available for vulkan-driver:'
contains provider '   1) nvidia-utils  2) vulkan-intel  3) vulkan-radeon'
contains provider "kyu: type 1/2/3 (Enter: pacman's default, 1)"
contains provider 'kyu: answered "Enter a number (default=1):" with 3'
contains provider 'installing vulkan-radeon...'
contains provider 'kyu: 3 upgraded, 1 installed'
log=$(ls "$work"/logs/*.jsonl | tail -n 1)
grep '"kind":"question"' "$log" | grep -q '"type":"provider"' || fail "question record lacks its type"
grep '"kind":"question"' "$log" | grep -q '"label":"vulkan-radeon"' || fail "question record lacks the options"

echo "Enter takes pacman's default"
INPUT='\n' expect_code 0 provider-default
contains provider-default 'installing nvidia-utils...'

echo "an invalid answer is refused and pacman keeps waiting"
INPUT='9\n2\n' expect_code 0 provider-invalid
contains provider-invalid "kyu: InvalidAnswer: \"9\" is not one of pacman's answers (1, 2, 3)"
contains provider-invalid 'installing vulkan-intel...'

echo "conflict: yes removes the other package"
mode conflict
INPUT='y\n' expect_code 0 conflict-yes
contains conflict-yes 'removing bar...'
contains conflict-yes 'kyu: 3 upgraded, 1 removed'

echo "conflict: pacman's default (no) ends the update with nothing changed"
INPUT='\n' expect_code 1 conflict-no
contains conflict-no 'kyu: pacman: failed to prepare transaction (conflicting dependencies). No packages were changed.'

echo "replacement declined: pacman goes on without it"
mode replace
INPUT='n\n' expect_code 0 replace-no
absent replace-no 'installing newthing...'
contains replace-no 'kyu: 3 upgraded'

echo "key import during the commit stage"
mode key
INPUT='y\n' expect_code 0 key
contains key ':: Import PGP key 0123ABCD, "Packager <p@example.org>"? [Y/n]'
contains key 'kyu: 3 upgraded'

echo "cancel at a question stops pacman before it changes anything"
mode conflict
INPUT='c\n' expect_code 1 cancelled
contains cancelled 'kyu: cancelled; stopping pacman'
contains cancelled "Cancelled at pacman's question \":: foo and bar are in conflict. Remove bar? [y/N]\". No packages were changed."
absent cancelled 'failed to prepare transaction'

echo "the D-Bus API: answers from another client, refusals"
mode provider
"$bin/kyu-update" --session </dev/null >"$work/remote.out" 2>&1 &
ui_pid=$!
wait_for "$work/remote.out" 'kyu: type'
tx=$(current_tx)
[ -n "$tx" ] || fail "no transaction id in GetState"
"$bin/kyu-update" --session --state | grep -q '"type": "provider"' || fail "GetState lacks the pending question"
# gdbus parses arguments as GVariant text: strings need their own quotes.
helper_call Answer "'$tx'" 1 "'9'" | grep -q 'Error.InvalidAnswer' || fail "invalid answer accepted"
helper_call Answer "'$tx'" 2 "'1'" | grep -q 'Error.NoQuestion' || fail "answer to a question never asked accepted"
helper_call Answer "'nope'" 1 "'1'" | grep -q 'Error.NoQuestion' || fail "answer to another transaction accepted"
helper_call Answer "'$tx'" 1 "'2'" | grep -qx '()' || fail "valid answer refused"
helper_call Answer "'$tx'" 1 "'2'" | grep -q 'Error.NoQuestion' || fail "question answered twice"
wait "$ui_pid" || fail "update answered over D-Bus failed"
grep -qF 'installing vulkan-intel...' "$work/remote.out" || fail "D-Bus answer not passed to pacman"
helper_call Cancel "'$tx'" | grep -q 'Error.NoQuestion' || fail "cancel accepted with no question pending"

echo "a client started while pacman waits sees the question and can answer it"
"$bin/kyu-update" --session </dev/null >"$work/first-ui.out" 2>&1 &
ui_pid=$!
wait_for "$work/first-ui.out" 'kyu: type'
kill -9 "$ui_pid"; wait "$ui_pid" 2>/dev/null || true
INPUT='3\n' expect_code 0 late --attach
contains late ':: There are 3 providers available for vulkan-driver:'
contains late 'installing vulkan-radeon...'
count late 'kyu: type 1/2/3' 1

echo "pacman failure is quoted with what had changed"
mode fail
expect_code 1 failed
contains failed 'kyu: pacman: failed to commit transaction (conflicting files). 2 upgraded before the error.'

echo "nothing to do"
mode nothing
expect_code 0 nothing
contains nothing 'kyu: Nothing to do'
absent nothing 'restart required'

echo "--attach after the fact replays the finished transaction"
expect_code 0 replay --attach
contains replay 'kyu: Nothing to do'

echo "SIGTERM during an update waits for pacman"
mode normal; delay 1
"$bin/kyu-update" --session >"$work/term.out" 2>&1 &
ui_pid=$!
wait_for "$work/term.out" 'upgrading kwin...'
kill -TERM "$helper_pid"
sleep 0.3
kill -0 "$helper_pid" 2>/dev/null || fail "helper exited mid-update on SIGTERM"
wait "$ui_pid" || fail "update did not finish after SIGTERM"
grep -qF 'kyu: 3 upgraded' "$work/term.out" || fail "update after SIGTERM did not complete"
i=0; while kill -0 "$helper_pid" 2>/dev/null; do i=$((i + 1)); [ $i -lt 50 ] || fail "helper did not exit after the update"; sleep 0.1; done
helper_pid=

echo "a restarted helper reports the previous transaction"
start_helper
expect_code 0 previous --attach
contains previous 'No update is running. Last update: 3 upgraded'

echo "an unanswered question times out and stops pacman"
kill -TERM "$helper_pid"; wait "$helper_pid" 2>/dev/null || true
start_helper --question-timeout 2
mode provider
expect_code 1 timeout
contains timeout 'kyu: no answer within 2 s; stopping pacman'
contains timeout 'No answer to "Enter a number (default=1):" within 2 seconds, so Kyu stopped pacman. No packages were changed.'
mode normal

echo "a helper killed mid-update leaves the transaction marked interrupted"
delay 1
"$bin/kyu-update" --session >"$work/crash.out" 2>&1 &
ui_pid=$!
wait_for "$work/crash.out" 'upgrading kwin...'
kill -9 "$helper_pid"
set +e; wait "$ui_pid"; ui_code=$?; set -e
[ "$ui_code" = 4 ] || fail "kyu-update exit code $ui_code after the helper died, expected 4"
grep -qF 'kyu: kyu-helper exited before the update finished' "$work/crash.out" || fail "crash not reported"
sleep 2.5   # let the orphaned fake pacman die on its closed pipe
helper_pid=
start_helper
expect_code 0 interrupted --attach
contains interrupted 'was interrupted: its log ends without a result'

echo "all helper tests passed"
