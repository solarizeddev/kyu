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

state() { # FILE PACKAGES FLATPAKS NEWS(0/1)
    news=""
    [ "$4" = 1 ] && news='"items":[{"title":"Manual intervention for foo","link":"https://archlinux.org/news/foo/","published":"2099-01-01T00:00:00Z"}],'
    pk=""; i=0
    while [ $i -lt "$2" ]; do pk="$pk${pk:+,}{\"name\":\"p$i\",\"repo\":\"core\",\"oldVersion\":\"1\",\"newVersion\":\"2\",\"size\":1}"; i=$((i + 1)); done
    fp=""; i=0
    while [ $i -lt "$3" ]; do fp="$fp${fp:+,}{\"name\":\"org.x.App$i\",\"kind\":\"app\",\"branch\":\"stable\",\"origin\":\"flathub\",\"installation\":\"user\"}"; i=$((i + 1)); done
    cat >"$1" <<JSON
{"formatVersion":1,"checkedAt":"2026-11-16T10:00:00Z",
 "pacman":{"checked":true,"ok":true,"checkedAt":"2026-11-16T10:00:00Z"},
 "flatpak":{"checked":true,"ok":true,"checkedAt":"2026-11-16T10:00:00Z"},
 "packages":[$pk],"flatpaks":[$fp],
 "news":{"checked":true,"ok":true,$news"source":"Arch Linux news","url":"https://archlinux.org/feeds/news/","page":"https://archlinux.org/news/"}}
JSON
}

# run NAME INPUT [kyu-update args...]: a fresh fake system per run.
run() {
    name=$1; input=$2; shift 2
    d="$work/$name"
    mkdir -p "$d/state" "$d/proc/sys/kernel" "$d/modules/6.18.22-1-MANJARO"
    : >"$d/pacman.log"
    # Manjaro's layout: the image is named by kernelbase, not pkgbase.
    echo "${RUNNING:-6.18.20-1-MANJARO}" >"$d/proc/sys/kernel/osrelease"
    echo "BOOT_IMAGE=/boot/vmlinuz-6.18-x86_64 rw" >"$d/proc/cmdline"
    touch "$d/modules/6.18.22-1-MANJARO/vmlinuz"
    echo linux618 >"$d/modules/6.18.22-1-MANJARO/pkgbase"
    echo 6.18-x86_64 >"$d/modules/6.18.22-1-MANJARO/kernelbase"
    [ -f "$d/state.json" ] || state "$d/state.json" 2 1 0
    printf "$input" | env PATH="$here/update:$PATH" KYU_FAKE_DIR="$d" XDG_STATE_HOME="$d/state" NO_COLOR=1 \
        timeout 30 "$bin/kyu-update" --no-wait --no-check --state-file "$d/state.json" --pacman-log "$d/pacman.log" \
        --db-lock "$d/db.lck" --proc-dir "$d/proc" --modules-dir "$d/modules" "$@" >"$d/out" 2>&1
    echo $? >"$d/code"
}
prepare() { mkdir -p "$work/$1"; }

echo "success: pacman, then flatpak, then what is left to do"
run ok 'y\n'
d=$work/ok
[ "$(cat "$d/code")" = 0 ] || fail "ok: exit $(cat "$d/code")"
has "$d/out" '$ sudo pacman -Syu' ok
has "$d/out" ':: Proceed with installation? [Y/n]' ok
has "$d/out" '$ flatpak update' ok
has "$d/out" 'kyu  Done: 2 packages, flatpak update finished.' ok
has "$d/out" 'kyu  Restart needed: kernel 6.18.20-1-MANJARO running, 6.18.22-1-MANJARO installed.' ok
has "$d/out" 'kyu  1 settings file to review:' ok
has "$d/out" '/etc/sddm.conf.pacnew' ok
has "$d/out" 'Do not close this window until Kyu says it is done.' ok
grep -q '"restartRequired": true' "$d/state/kyu/last-run.json" || fail "ok: last-run.json without the restart"
grep -q '"/etc/sddm.conf.pacnew"' "$d/state/kyu/last-run.json" || fail "ok: last-run.json without the settings file"

echo "news: Enter alone (or n) stops before anything runs"
prepare news-no; state "$work/news-no/state.json" 2 1 1
run news-no '\n'
d=$work/news-no
[ "$(cat "$d/code")" = 4 ] || fail "news-no: exit $(cat "$d/code")"
has "$d/out" 'Manual intervention for foo' news-no
has "$d/out" 'Continue? [y/N]' news-no
has "$d/out" 'Stopped. Nothing was changed.' news-no
[ ! -e "$d/pacman-ran" ] || fail "news-no: pacman ran"

echo "news: y continues"
prepare news-yes; state "$work/news-yes/state.json" 2 1 1
run news-yes 'y\ny\n'
[ -e "$work/news-yes/pacman-ran" ] || fail "news-yes: pacman did not run"
# The second "y" reached pacman's own question (the fake stops without one).
has "$work/news-yes/out" 'kyu  Done:' news-yes

echo "failure: keyring, explained, Flatpaks skipped"
KYU_FAKE_MODE=keyring run keyring 'y\n'
d=$work/keyring
[ "$(cat "$d/code")" = 1 ] || fail "keyring: exit $(cat "$d/code")"
has "$d/out" 'kyu  pacman failed (exit status 1). No packages were changed. Flatpaks were not updated.' keyring
has "$d/out" 'kyu  Package signatures could not be checked.' keyring
has "$d/out" 'sudo pacman -Sy --needed' keyring
[ ! -e "$d/flatpak-ran" ] || fail "keyring: flatpak ran after pacman failed"
grep -q '"failure": "keyring"' "$d/state/kyu/last-run.json" || fail "keyring: last-run.json without the failure"

echo "pacman's own Proceed answered n: nothing changed, no explanation invented"
run decline 'n\n'
d=$work/decline
has "$d/out" 'kyu  pacman stopped before changing anything (exit status 1).' decline
hasnt "$d/out" 'usually fixes it' decline

echo "stale lock: said, with the command; nothing runs"
prepare lock; touch "$work/lock/db.lck"
run lock ''
d=$work/lock
[ "$(cat "$d/code")" = 1 ] || fail "lock: exit $(cat "$d/code")"
has "$d/out" "pacman's lock file is left over" lock
has "$d/out" "sudo rm $d/db.lck" lock
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
has "$d/out" 'kyu  Done: flatpak update finished.' fponly

echo "a stale lock while the pamac tray runs (it takes no lock)"
prepare tray; touch "$work/tray/db.lck"; mkdir -p "$work/tray/proc/777"; echo pamac-tray-plas >"$work/tray/proc/777/comm"
run tray ''
has "$work/tray/out" "pacman's lock file is left over" tray

echo "flatpak fails after pacman: both said"
KYU_FAKE_FLATPAK_EXIT=1 run fpfail 'y\n'
d=$work/fpfail
[ "$(cat "$d/code")" = 5 ] || fail "fpfail: exit $(cat "$d/code")"
has "$d/out" 'kyu  flatpak update failed (exit status 1)' fpfail
has "$d/out" "kyu  pacman's part is done: 2 packages." fpfail

echo "pacman fails mid-transaction: what changed is said"
KYU_FAKE_MODE=midfail run midfail 'y\n'
has "$work/midfail/out" 'kyu  pacman failed (exit status 1) after changing 1 package. Flatpaks were not updated.' midfail

echo "another language: no invented explanation"
KYU_FAKE_MODE=german run german ''
has "$work/german/out" 'kyu  pacman stopped before changing anything (exit status 1).' german
has "$work/german/out" "kyu  pacman's last lines:" german
has "$work/german/out" 'Fehler: Konnte nicht alle Datenbanken aktualisieren' german
hasnt "$work/decline/out" "pacman's last lines" decline

echo "a restart from systemd is not forgotten by a later run (same boot)"
RUNNING=6.18.22-1-MANJARO KYU_FAKE_MODE=systemd run carry 'y\n'
has "$work/carry/out" 'kyu  Restart needed: systemd was updated.' carry
RUNNING=6.18.22-1-MANJARO KYU_FAKE_MODE=nothing run carry ''
has "$work/carry/out" 'kyu  Done: flatpak update finished.' carry-2
hasnt "$work/carry/out" 'Done: 1 package' carry-2
if [ -r /proc/sys/kernel/random/boot_id ]; then
    has "$work/carry/out" 'kyu  Restart needed: systemd was updated.' carry-2
fi

echo "one update at a time"
KYU_FAKE_MODE=slow run first 'y\n' &
first=$!
sleep 1
XDG_STATE_HOME="$work/first/state" NO_COLOR=1 "$bin/kyu-update" --no-wait --no-check --state-file "$work/first/state.json" \
    </dev/null >"$work/second.out" 2>&1
second=$?
wait $first
[ "$second" = 3 ] || fail "second run: exit $second, expected 3"
has "$work/second.out" 'An update is already running' second

[ $fails -eq 0 ] && echo "all kyu-update tests passed"
[ $fails -eq 0 ]
