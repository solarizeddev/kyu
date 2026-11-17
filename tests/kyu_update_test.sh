#!/bin/sh
# End-to-end test of kyu-update, the update in the terminal: the real
# kyu-update and util-linux `script`, with fake sudo, pacman and flatpak
# first on PATH (tests/update/), a fake pacman.log, /proc and modules dir.
#
#   sh tests/kyu_update_test.sh BUILD_BIN_DIR
set -u
bin=$(cd "$1" && pwd)
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap '[ -n "${KEEP:-}" ] || rm -rf "$work"' EXIT
fails=0

fail() { echo "FAIL: $*"; fails=$((fails + 1)); }
has() { grep -qF -- "$2" "$1" || { fail "$3: expected '$2' in output"; sed 's/^/    /' "$1"; }; }
hasnt() { if grep -qF -- "$2" "$1"; then fail "$3: did not expect '$2'"; sed 's/^/    /' "$1"; fi; }

. "$here/update/fake-system.sh"
state() { fake_state "$@"; }

# run NAME INPUT [kyu-update args...]: a fresh fake system per run.
run() {
    name=$1; input=$2; shift 2
    d="$work/$name"
    fake_system "$d"
    printf "$input" | env PATH="$here/update:$PATH" KYU_FAKE_DIR="$d" XDG_STATE_HOME="$d/state" NO_COLOR=1 LC_ALL="${LC_ALL:-C.UTF-8}" \
        timeout 30 "$bin/kyu-update" --no-wait $(fake_args "$d") "$@" >"$d/out" 2>&1
    echo $? >"$d/code"
}
prepare() { mkdir -p "$work/$1"; }
# idle FILE: until FILE has not grown for 0.6 seconds (at most 15 seconds).
idle() {
    last=-1; n=0
    while [ $n -lt 50 ]; do
        sleep 0.3; n=$((n + 1))
        size=$(wc -c <"$1" 2>/dev/null || echo 0)
        [ "$size" -gt 0 ] && [ "$size" = "$last" ] && sleep 0.3 && [ "$(wc -c <"$1")" = "$size" ] && return
        last=$size
    done
}

echo "success: pacman, then flatpak, then what is left to do"
run ok 'y\n'
d=$work/ok
[ "$(cat "$d/code")" = 0 ] || fail "ok: exit $(cat "$d/code")"
has "$d/out" 'Packages   sudo pacman -Syu' ok
has "$d/out" ':: Proceed with installation? [Y/n]' ok
has "$d/out" 'Flatpaks   flatpak update' ok
has "$d/out" ' · 2 packages · 1 Flatpak' ok
has "$d/out" 'Done in ' ok
has "$d/out" 'Restart needed: new kernel 6.18.22-1-MANJARO' ok
has "$d/out" '1 settings file changed: /etc/sddm.conf' ok
hasnt "$d/out" 'Nothing else to do' ok
has "$d/out" 'Keep this window open until Kyu says it is done.' ok
hasnt "$d/out" 'Snapshot' ok
grep -q '"restartRequired": true' "$d/state/kyu/last-run.json" || fail "ok: last-run.json without the restart"
[ -e "$d/inhibited" ] || fail "ok: pacman did not run under systemd-inhibit"
[ "$(cat "$d/pacman-args")" = -Syu ] || fail "ok: pacman ran with $(cat "$d/pacman-args")"
grep -q '"/etc/sddm.conf.pacnew"' "$d/state/kyu/last-run.json" || fail "ok: last-run.json without the settings file"

echo "news: Enter alone (or n) stops before anything runs"
prepare news-no; state "$work/news-no/state.json" 2 1 1
run news-no '\n'
d=$work/news-no
[ "$(cat "$d/code")" = 4 ] || fail "news-no: exit $(cat "$d/code")"
has "$d/out" 'Manual intervention for foo' news-no
has "$d/out" 'Continue? [y/N]' news-no
has "$d/out" 'Stopped. Nothing was installed.' news-no
[ ! -e "$d/pacman-ran" ] || fail "news-no: pacman ran"
[ ! -e "$d/state/kyu/news-seen.json" ] || fail "news-no: marked as read without a y"

echo "news: y continues"
prepare news-yes; state "$work/news-yes/state.json" 2 1 1
run news-yes 'y\ny\n'
[ -e "$work/news-yes/pacman-ran" ] || fail "news-yes: pacman did not run"
# The second "y" reached pacman's own question (the fake stops without one).
has "$work/news-yes/out" 'Done in ' news-yes

echo "news: asked about once; afterwards only listed"
prepare newsonce; state "$work/newsonce/state.json" 2 1 1
run newsonce 'y\ny\n'
has "$work/newsonce/out" 'Continue? [y/N]' newsonce
grep -q 'archlinux.org/news/foo' "$work/newsonce/state/kyu/news-seen.json" || fail "newsonce: not remembered as seen"
run newsonce 'y\n'
has "$work/newsonce/out" 'News (read): Manual intervention for foo · https://archlinux.org/news/foo/' newsonce-2
hasnt "$work/newsonce/out" 'Continue? [y/N]' newsonce-2
has "$work/newsonce/out" 'Done in ' newsonce-2

echo "news: one read, one new: only the new one is asked about"
prepare newsmixed; state "$work/newsmixed/state.json" 2 1 1
sed -i 's#"items":\[{#"items":[{"title":"Second thing","link":"https://archlinux.org/news/second/","published":"2099-01-02T00:00:00Z"},{#' "$work/newsmixed/state.json"
mkdir -p "$work/newsmixed/state/kyu"
printf '{"seen":["https://archlinux.org/news/foo/"]}' >"$work/newsmixed/state/kyu/news-seen.json"
run newsmixed 'y\ny\n'
has "$work/newsmixed/out" 'Second thing' newsmixed
hasnt "$work/newsmixed/out" 'Manual intervention for foo' newsmixed
has "$work/newsmixed/out" 'Continue? [y/N]' newsmixed

echo "failure: keyring, explained, Flatpaks skipped"
KYU_FAKE_MODE=keyring run keyring 'y\n'
d=$work/keyring
[ "$(cat "$d/code")" = 1 ] || fail "keyring: exit $(cat "$d/code")"
has "$d/out" 'Nothing was installed. Flatpaks were skipped.' keyring
has "$d/out" '  Package signatures could not be checked.' keyring
[ ! -e "$d/flatpak-ran" ] || fail "keyring: flatpak ran after pacman failed"
grep -q '"failure": "keyring"' "$d/state/kyu/last-run.json" || fail "keyring: last-run.json without the failure"

echo "pacman's own Proceed answered n: nothing changed, no explanation invented"
run decline 'n\n'
d=$work/decline
has "$d/out" 'Stopped. Nothing was installed.' decline

echo "stale lock: said, with the command; nothing runs"
prepare lock; touch "$work/lock/db.lck"
run lock ''
d=$work/lock
[ "$(cat "$d/code")" = 1 ] || fail "lock: exit $(cat "$d/code")"
has "$d/out" "pacman's lock is left over" lock
[ ! -e "$d/pacman-ran" ] || fail "lock: pacman ran"

echo "lock held by a running pacman"
prepare busy; touch "$work/busy/db.lck"; mkdir -p "$work/busy/proc/4242"; echo pacman >"$work/busy/proc/4242/comm"
run busy ''
has "$work/busy/out" 'Another package manager is running (pacman, pid 4242)' busy

echo "Flatpaks only: no sudo, no pacman"
prepare fponly; state "$work/fponly/state.json" 0 2 0
run fponly ''
d=$work/fponly
[ ! -e "$d/pacman-ran" ] || fail "fponly: pacman ran"
[ -e "$d/flatpak-ran" ] || fail "fponly: flatpak did not run"
has "$d/out" ' · 2 Flatpaks' fponly

echo "a stale lock while the pamac tray runs (it takes no lock)"
prepare tray; touch "$work/tray/db.lck"; mkdir -p "$work/tray/proc/777"; echo pamac-tray-plas >"$work/tray/proc/777/comm"
run tray ''
has "$work/tray/out" "pacman's lock is left over" tray

echo "flatpak fails after pacman: both said"
KYU_FAKE_FLATPAK_EXIT=1 run fpfail 'y\n'
d=$work/fpfail
[ "$(cat "$d/code")" = 5 ] || fail "fpfail: exit $(cat "$d/code")"
has "$d/out" 'Flatpaks failed (status 1)' fpfail
has "$d/out" 'Packages done: 2 packages' fpfail

echo "pacman fails mid-transaction: what changed is said"
KYU_FAKE_MODE=midfail run midfail 'y\n'
has "$work/midfail/out" 'pacman failed after changing 1 package. Flatpaks were skipped.' midfail

echo "another language: no invented explanation"
KYU_FAKE_MODE=german run german ''
has "$work/german/out" 'Nothing was installed. Flatpaks were skipped.' german
has "$work/german/out" "pacman's last lines (status 1):" german
has "$work/german/out" 'Fehler: Konnte nicht alle Datenbanken aktualisieren' german
hasnt "$work/decline/out" "pacman's last lines" decline

echo "a restart from systemd is not forgotten by a later run (same boot)"
RUNNING=6.18.22-1-MANJARO KYU_FAKE_MODE=systemd run carry 'y\n'
has "$work/carry/out" 'Restart needed: systemd was updated' carry
RUNNING=6.18.22-1-MANJARO KYU_FAKE_MODE=nothing run carry ''
has "$work/carry/out" ' · 1 Flatpak' carry-2
hasnt "$work/carry/out" '· 1 package' carry-2
if [ -r /proc/sys/kernel/random/boot_id ]; then
    has "$work/carry/out" 'Restart needed: systemd was updated' carry-2
fi

echo "up to date: nothing else to do"
prepare clean; state "$work/clean/state.json" 0 0 0
RUNNING=6.18.22-1-MANJARO KYU_FAKE_MODE=nothing run clean ''
has "$work/clean/out" 'Already up to date.' clean
has "$work/clean/out" 'Nothing else to do.' clean
hasnt "$work/clean/out" 'Restart needed' clean

echo "a snapshot hook is named before pacman"
prepare snap; mkdir -p "$work/snap/hooks"; touch "$work/snap/hooks/05-snap-pac-pre.hook"
run snap 'y\n'
has "$work/snap/out" 'Snapshot   snap-pac takes one first' snap

# runtty NAME INPUT [args...]: in a terminal (util-linux script), so the last
# line offers its keys. INPUT is answers separated by "|", each typed once the
# output has been still for a moment (Kyu drops keys typed before its question).
runtty() {
    name=$1; input=$2; shift 2
    d="$work/$name"
    fake_system "$d"
    # shellcheck disable=SC2046
    : >"$d/out"
    (IFS='|'; for a in $input; do idle "$d/out"; printf '%b' "$a"; done; idle "$d/out"; sleep 1) | env PATH="$here/update:$PATH" KYU_FAKE_DIR="$d" XDG_STATE_HOME="$d/state" NO_COLOR=1 LC_ALL="${LC_ALL:-C.UTF-8}" \
        timeout 30 script -q -e -c "$bin/kyu-update $(fake_args "$d") $*" /dev/null >"$d/out" 2>&1
    echo $? >"$d/code"
}

echo "keys: [f] removes a stale lock, then the update runs again"
prepare lockfix; touch "$work/lockfix/db.lck"
runtty lockfix 'f\n|y\n|\n'
d=$work/lockfix
has "$d/out" "Fix: sudo rm $d/db.lck" lockfix
has "$d/out" '[f] fix, update again   [Enter] close' lockfix
[ ! -e "$d/db.lck" ] || fail "lockfix: the lock is still there"
[ -e "$d/pacman-ran" ] || fail "lockfix: the update did not run again"
has "$d/out" 'Done in ' lockfix
[ "$(cat "$d/code")" = 0 ] || fail "lockfix: exit $(cat "$d/code")"

echo "keys: a keyring failure offers its fix; [m] runs pacdiff; Enter closes"
KYU_FAKE_MODE=keyring runtty keyfix 'y\n|\n'
has "$work/keyfix/out" 'Fix: sudo pacman -Sy --needed archlinux-keyring manjaro-keyring' keyfix
has "$work/keyfix/out" '[Enter] close' keyfix
runtty merge 'y\n|m\n|\n'
d=$work/merge
has "$d/out" '[r] restart   [m] merge   [Enter] close' merge
[ -e "$d/pacdiff-ran" ] || fail "merge: pacdiff did not run"
has "$d/out" 'fake pacdiff --sudo' merge

echo "plain ASCII where the terminal is not UTF-8"
LC_ALL=C run ascii 'y\n'
has "$work/ascii/out" 'Kyu - 3 updates: 2 packages, 1 Flatpak' ascii
has "$work/ascii/out" '> Packages   sudo pacman -Syu' ascii

echo "--refresh: pacman -Syyu"
run refresh 'y\n' --refresh
[ "$(cat "$work/refresh/pacman-args")" = -Syyu ] || fail "refresh: pacman ran with $(cat "$work/refresh/pacman-args")"

echo "keys: [r] asks, then restarts (here: a fake systemctl)"
runtty reboot 'y\n|r\n|y\n'
[ -e "$work/reboot/systemctl-reboot" ] || fail "reboot: systemctl reboot did not run"
has "$work/reboot/out" 'Restart now? Unsaved work in open programs is lost. [y/N]' reboot

echo "keys: no [r] after half an update"
KYU_FAKE_MODE=midfail runtty halfway 'y\n|\n'
has "$work/halfway/out" 'Do not restart until the update has finished.' halfway
hasnt "$work/halfway/out" '[r] restart' halfway

echo "keys: [f] for the keyring, then the whole update again without the news"
prepare keyagain; state "$work/keyagain/state.json" 2 1 1
KYU_FAKE_MODE=keyring runtty keyagain 'y\n|y\n|f\n|y\n|\n'
d=$work/keyagain
[ -e "$d/keyring-fixed" ] || fail "keyagain: the keyring fix did not run"
[ "$(grep -c 'Continue? \[y/N\]' "$d/out")" = 1 ] || fail "keyagain: the news were asked again"
has "$d/out" 'The package lists are newer than the system: update before installing anything.' keyagain

echo "a restart from before this run says so"
prepare earlier; state "$work/earlier/state.json" 0 0 0
KYU_FAKE_MODE=nothing run earlier ''
has "$work/earlier/out" 'Restart needed: new kernel 6.18.22-1-MANJARO (from an earlier update)' earlier

echo "--merge: pacdiff only, then the record keeps what is left"
run mergeonly 'y\n'
d=$work/mergeonly
: >"$d/pacdiff-ran.before"
rm -f "$d/pacdiff-ran"
run mergeonly '' --merge
has "$d/out" 'Kyu · settings files' mergeonly
[ -e "$d/pacdiff-ran" ] || fail "mergeonly: pacdiff did not run"
grep -q 'sddm.conf.pacnew' "$d/state/kyu/last-run.json" && fail "mergeonly: a merged file is still listed" || true

echo "--merge with no earlier run: pacdiff, nothing else"
prepare mergefresh
run mergefresh '' --merge
[ -e "$work/mergefresh/pacdiff-ran" ] || fail "mergefresh: pacdiff did not run"
[ ! -e "$work/mergefresh/state/kyu/last-run.json" ] || fail "mergefresh: wrote a record from nothing"
[ ! -e "$work/mergefresh/state/kyu/activity" ] || fail "mergefresh: left its activity file"

echo "one update at a time"
KYU_FAKE_MODE=slow run first 'y\n' &
first=$!
sleep 1
XDG_STATE_HOME="$work/first/state" NO_COLOR=1 "$bin/kyu-update" --no-wait --no-check --state-file "$work/first/state.json" \
    </dev/null >"$work/second.out" 2>&1
second=$?
XDG_STATE_HOME="$work/first/state" NO_COLOR=1 PATH="$here/update:$PATH" KYU_FAKE_DIR="$work/first" "$bin/kyu-update" --no-wait --no-check --merge \
    --state-file "$work/first/state.json" </dev/null >"$work/merge-during.out" 2>&1
merging=$?
wait $first
[ "$merging" = 3 ] || fail "--merge during an update: exit $merging, expected 3"
[ "$second" = 3 ] || fail "second run: exit $second, expected 3"
has "$work/second.out" 'An update is already running' second

[ $fails -eq 0 ] && echo "all kyu-update tests passed"
[ $fails -eq 0 ]
