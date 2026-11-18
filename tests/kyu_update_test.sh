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

# run NAME INPUT [kyu-update args...]: a fresh fake system per run. In the
# C.UTF-8 locale, or LC_ALL's; with KYU_TEST_NO_LC_ALL=1, LC_ALL empty (LANG
# and LANGUAGE decide).
run() {
    name=$1; input=$2; shift 2
    d="$work/$name"
    fake_system "$d"
    lc_all=${LC_ALL:-C.UTF-8}
    [ -z "${KYU_TEST_NO_LC_ALL:-}" ] || lc_all=
    printf "$input" | env PATH="$here/update:$PATH" KYU_TEST_UNUSED_FLATPAKS="${KYU_TEST_UNUSED_FLATPAKS:-}" KYU_FAKE_DIR="$d" XDG_STATE_HOME="$d/state" NO_COLOR=1 LC_ALL="$lc_all" \
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
has "$d/out" '✓ Done' ok
has "$d/out" 'Restart required: new kernel 6.18.22-1-MANJARO' ok
has "$d/out" '1 configuration file to merge: /etc/sddm.conf' ok
hasnt "$d/out" 'Nothing else to do' ok
has "$d/out" 'Keep this window open until Kyu says it is done.' ok
hasnt "$d/out" 'Snapshot' ok
hasnt "$d/out" 'AUR' ok
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
has "$d/out" 'Stopped. No packages were upgraded.' news-no
[ ! -e "$d/pacman-ran" ] || fail "news-no: pacman ran"
[ ! -e "$d/state/kyu/news-seen.json" ] || fail "news-no: marked as read without a y"

echo "news: y continues"
prepare news-yes; state "$work/news-yes/state.json" 2 1 1
run news-yes 'y\ny\n'
[ -e "$work/news-yes/pacman-ran" ] || fail "news-yes: pacman did not run"
# The second "y" reached pacman's own question (the fake stops without one).
has "$work/news-yes/out" '✓ Done' news-yes

echo "news: asked about once; afterwards only listed"
prepare newsonce; state "$work/newsonce/state.json" 2 1 1
run newsonce 'y\ny\n'
has "$work/newsonce/out" 'Continue? [y/N]' newsonce
grep -q 'archlinux.org/news/foo' "$work/newsonce/state/kyu/news-seen.json" || fail "newsonce: not remembered as seen"
run newsonce 'y\n'
has "$work/newsonce/out" 'News (read): Manual intervention for foo · https://archlinux.org/news/foo/' newsonce-2
hasnt "$work/newsonce/out" 'Continue? [y/N]' newsonce-2
has "$work/newsonce/out" '✓ Done' newsonce-2

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
has "$d/out" 'No packages were upgraded. Flatpaks were skipped.' keyring
has "$d/out" '  Package signatures could not be checked.' keyring
# The cause right under the verdict; the partial-upgrade warning after it.
sed -n '/No packages were upgraded/{n;p;q}' "$d/out" | grep -qF 'Package signatures could not be checked.' || fail "keyring: the cause is not under the verdict"
[ "$(grep -nF 'Package signatures' "$d/out" | cut -d: -f1 | head -1)" -lt "$(grep -nF 'newer than the system' "$d/out" | cut -d: -f1 | head -1)" ] \
    || fail "keyring: the partial-upgrade warning comes before the cause"
[ ! -e "$d/flatpak-ran" ] || fail "keyring: flatpak ran after pacman failed"
grep -q '"failure": "keyring"' "$d/state/kyu/last-run.json" || fail "keyring: last-run.json without the failure"

echo "pacman's own Proceed answered n: nothing changed, no explanation invented"
run decline 'n\n'
d=$work/decline
has "$d/out" 'Stopped. No packages were upgraded.' decline
has "$d/out" 'The package lists are newer than the system: update before installing anything.' decline

echo "Ctrl+C at Proceed: the lists changed, so the warning"
KYU_FAKE_MODE=interrupt run ctrlc ''
has "$work/ctrlc/out" 'The package lists are newer than the system' ctrlc
# A stop on purpose, as "n" is (seen with a real pacman: it exits 130).
has "$work/ctrlc/out" 'Stopped. No packages were upgraded.' ctrlc
hasnt "$work/ctrlc/out" "pacman's last lines" ctrlc

echo "a dependency pacman cannot satisfy: a failure, not a stop (its last line starts with ::)"
KYU_FAKE_MODE=deps run deps ''
has "$work/deps/out" 'Dependencies cannot be satisfied.' deps
hasnt "$work/deps/out" 'Stopped.' deps
grep -q '"stopped"' "$work/deps/state/kyu/last-run.json" && fail "deps: recorded as stopped"

echo "a stop is recorded as one (the card does not call it a failed update)"
grep -q '"stopped": true' "$work/ctrlc/state/kyu/last-run.json" || fail "ctrlc: not recorded as stopped"

echo "sudo refuses (three wrong passwords): stopped, sudo's line, no lists warning"
KYU_FAKE_SUDO=refuse run sudono ''
has "$work/sudono/out" 'Stopped. No packages were upgraded.' sudono
has "$work/sudono/out" 'sudo: 3 incorrect password attempts' sudono
hasnt "$work/sudono/out" "pacman's last lines" sudono
hasnt "$work/sudono/out" 'newer than the system' sudono

echo "stale lock: said, with the command; nothing runs"
prepare lock; touch "$work/lock/db.lck"
run lock ''
d=$work/lock
[ "$(cat "$d/code")" = 1 ] || fail "lock: exit $(cat "$d/code")"
has "$d/out" "pacman's lock is left over" lock
[ ! -e "$d/pacman-ran" ] || fail "lock: pacman ran"

echo "lock held by a running pacman: waited for, then given up"
prepare busy; touch "$work/busy/db.lck"; mkdir -p "$work/busy/proc/4242"; echo pacman >"$work/busy/proc/4242/comm"
run busy '' --lock-wait 2
[ "$(cat "$work/busy/code")" = 1 ] || fail "busy: exit $(cat "$work/busy/code")"
has "$work/busy/out" 'Waiting for pacman (pid 4242) to finish. Press Ctrl+C to cancel.' busy
has "$work/busy/out" 'Still busy after 2 s. Update when it is done.' busy
[ ! -e "$work/busy/pacman-ran" ] || fail "busy: pacman ran"

echo "lock held by pamac: waited for, then the update goes on"
prepare busywait; touch "$work/busywait/db.lck"; mkdir -p "$work/busywait/proc/4243"; echo pamac-daemon >"$work/busywait/proc/4243/comm"
: >"$work/busywait/out"
(for i in $(seq 200); do grep -q 'Waiting for' "$work/busywait/out" 2>/dev/null && break; sleep 0.1; done; rm -f "$work/busywait/db.lck") &
run busywait 'y\n' --lock-wait 20
wait
has "$work/busywait/out" 'Waiting for pamac-daemon (pid 4243) to finish.' busywait
has "$work/busywait/out" '✓ Done' busywait
[ -e "$work/busywait/pacman-ran" ] || fail "busywait: pacman did not run"

echo "a holder that does nothing for a while: the lock looks left over"
prepare idle; touch "$work/idle/db.lck"; mkdir -p "$work/idle/proc/4245"; echo pamac-daemon >"$work/idle/proc/4245/comm"
run idle '' --lock-wait 20 --idle-after 1
has "$work/idle/out" "pacman's lock looks left over: pamac-daemon is running but idle." idle
[ ! -e "$work/idle/pacman-ran" ] || fail "idle: pacman ran"

echo "managers that all look younger than the lock (a clock step?): waited for, then only 'idle'"
prepare young; touch "$work/young/db.lck"; mkdir -p "$work/young/proc/4246"; echo pamac-daemon >"$work/young/proc/4246/comm"
echo "btime $(( $(date +%s) + 3600 ))" >"$work/young/proc/stat"
echo "4246 (pamac-daemon) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 0" >"$work/young/proc/4246/stat"
run young '' --lock-wait 20 --idle-after 1
has "$work/young/out" "pacman's lock looks left over: pamac-daemon is running but idle." young

echo "pacman itself is never called idle (it may sit at its own question)"
prepare pacmanq; touch "$work/pacmanq/db.lck"; mkdir -p "$work/pacmanq/proc/4247"; echo pacman >"$work/pacmanq/proc/4247/comm"
run pacmanq '' --lock-wait 3 --idle-after 1
has "$work/pacmanq/out" 'Still busy after 3 s.' pacmanq
hasnt "$work/pacmanq/out" 'looks left over' pacmanq

echo "a holder with a child (a long hook) is never called idle"
prepare hook; touch "$work/hook/db.lck"; mkdir -p "$work/hook/proc/4248" "$work/hook/proc/4249"
echo pamac-daemon >"$work/hook/proc/4248/comm"; echo dkms >"$work/hook/proc/4249/comm"
echo "4249 (dkms) S 4248 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 0" >"$work/hook/proc/4249/stat"
run hook '' --lock-wait 3 --idle-after 1
has "$work/hook/out" 'Still busy after 3 s.' hook

echo "no package manager at all: left over at once"
prepare orphanlock; touch "$work/orphanlock/db.lck"
run orphanlock '' --lock-wait 20
has "$work/orphanlock/out" "pacman's lock is left over from a stopped run; nothing is using it." orphanlock
hasnt "$work/orphanlock/out" 'Waiting for' orphanlock

echo "other users' processes hidden (hidepid): say so; root decides"
prepare hidden; touch "$work/hidden/db.lck"
run hidden '' --lock-wait 20 --proc-dir "$work/hidden/noproc"
has "$work/hidden/out" "Kyu cannot see other users' programs" hidden

echo "Flatpaks only: pacman's lock does not matter"
prepare fplock; state "$work/fplock/state.json" 0 2 0; touch "$work/fplock/db.lck"
run fplock ''
has "$work/fplock/out" 'Flatpaks   flatpak update' fplock
hasnt "$work/fplock/out" "pacman's lock" fplock

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
has "$d/out" ' · 2 packages' fpfail

echo "pacman fails mid-transaction: what changed is said"
KYU_FAKE_MODE=midfail run midfail 'y\n'
has "$work/midfail/out" 'pacman failed after changing 1 package. Flatpaks were skipped.' midfail

echo "another language: no invented explanation"
KYU_FAKE_MODE=german run german ''
has "$work/german/out" 'No packages were upgraded. Flatpaks were skipped.' german
has "$work/german/out" "pacman's last lines (status 1):" german
has "$work/german/out" 'Fehler: Konnte nicht alle Datenbanken aktualisieren' german
hasnt "$work/decline/out" "pacman's last lines" decline

# Kyu's German (translations/kyu_de.ts), when lrelease built it: Kyu speaks
# German under de_DE too (Qt reads LANG, LC_ALL and LANGUAGE itself, even
# where the de_DE locale is not generated, as here).
qm="$bin/../translations"
if [ -e "$qm/kyu_de.qm" ]; then
    de_signatures='Paketsignaturen konnten nicht überprüft werden.'; de_fix='Abhilfe: sudo pacman'; de_last='Die letzten Zeilen von pacman (Status 1):'
else
    echo "(no kyu_de.qm: the German runs check English)"
    de_signatures='Package signatures could not be checked.'; de_fix='Fix: sudo pacman'; de_last="pacman's last lines (status 1):"
fi

echo "another language: recognised with pacman's own translations, shown as printed"
LC_ALL=de_DE.UTF-8 KYU_FAKE_MODE=germankeyring run germankey 'y\n' --locale-dir "$here/update/locale"
d=$work/germankey
has "$d/out" 'Fehler: Der Vorgang konnte nicht durchgef' germankey
has "$d/out" "$de_signatures" germankey
has "$d/out" "$de_fix -Sy --needed archlinux-keyring manjaro-keyring" germankey
grep -q '"failure": "keyring"' "$d/state/kyu/last-run.json" || fail "germankey: last-run.json without the failure"
# Without pacman's catalogs for it: pacman's own last lines, nothing invented.
LC_ALL=de_DE.UTF-8 KYU_FAKE_MODE=germankeyring run germannone 'y\n' --locale-dir "$work/none"
has "$work/germannone/out" "$de_last" germannone
hasnt "$work/germannone/out" "$de_signatures" germannone

echo "a restart from systemd is not forgotten by a later run (same boot)"
RUNNING=6.18.22-1-MANJARO KYU_FAKE_MODE=systemd run carry 'y\n'
has "$work/carry/out" 'Restart required: systemd was updated' carry
RUNNING=6.18.22-1-MANJARO KYU_FAKE_MODE=nothing run carry ''
has "$work/carry/out" ' · 1 Flatpak' carry-2
hasnt "$work/carry/out" '· 1 package' carry-2
if [ -r /proc/sys/kernel/random/boot_id ]; then
    has "$work/carry/out" 'Restart required: systemd was updated' carry-2
fi

echo "up to date: nothing else to do"
prepare clean; state "$work/clean/state.json" 0 0 0
RUNNING=6.18.22-1-MANJARO KYU_FAKE_MODE=nothing run clean ''
has "$work/clean/out" 'Already up to date.' clean
hasnt "$work/clean/out" 'Nothing else to do' clean
hasnt "$work/clean/out" 'Restart required' clean

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
    (IFS='|'; for a in $input; do idle "$d/out"; printf '%b' "$a"; done; idle "$d/out"; sleep 1) | env PATH="$here/update:$PATH" KYU_TEST_UNUSED_FLATPAKS="${KYU_TEST_UNUSED_FLATPAKS:-}" KYU_FAKE_DIR="$d" XDG_STATE_HOME="$d/state" NO_COLOR=1 LC_ALL="${LC_ALL:-C.UTF-8}" \
        timeout 30 script -q -e -c "$bin/kyu-update $(fake_args "$d") $*" /dev/null >"$d/out" 2>&1
    echo $? >"$d/code"
}

echo "waiting: pacman's question is noted for the Kyu window, and cleared once answered"
prepare waiting
d=$work/waiting
fake_system "$d"
: >"$d/out"
(
    n=0
    while [ ! -s "$d/state/kyu/waiting" ] && [ $n -lt 40 ]; do sleep 0.25; n=$((n + 1)); done
    cp "$d/state/kyu/waiting" "$d/seen" 2>/dev/null
    printf 'y\n'; idle "$d/out"; sleep 1
) | env PATH="$here/update:$PATH" KYU_TEST_UNUSED_FLATPAKS="${KYU_TEST_UNUSED_FLATPAKS:-}" KYU_FAKE_DIR="$d" XDG_STATE_HOME="$d/state" NO_COLOR=1 LC_ALL="${LC_ALL:-C.UTF-8}" \
    timeout 30 script -q -e -c "$bin/kyu-update --no-wait $(fake_args "$d")" /dev/null >"$d/out" 2>&1
[ "$(cat "$d/seen" 2>/dev/null)" = 'Proceed with installation? [Y/n]' ] || fail "waiting: noted '$(cat "$d/seen" 2>/dev/null)'"
[ ! -e "$d/state/kyu/waiting" ] || fail "waiting: still noted after the update"
has "$d/out" '✓ Done' waiting

echo "flatpak's own question answered n: a stop, not a failure (seen with a real flatpak)"
KYU_FAKE_FLATPAK=ask runtty fpno 'y\n|n\n|\n'
has "$work/fpno/out" 'Flatpaks: stopped. None were updated.' fpno
hasnt "$work/fpno/out" 'Flatpaks failed' fpno
hasnt "$work/fpno/out" ' · 1 Flatpak' fpno

echo "flatpak finds nothing though the check listed one (a remote gone): not called done"
KYU_FAKE_FLATPAK=nothing runtty fpnone 'y\n|\n'
has "$work/fpnone/out" 'Flatpaks not updated: flatpak found nothing to update' fpnone
hasnt "$work/fpnone/out" ' · 1 Flatpak' fpnone

echo "keys: [f] removes a stale lock, then the update runs again"
prepare lockfix; touch "$work/lockfix/db.lck"
runtty lockfix 'f\n|y\n|\n'
d=$work/lockfix
has "$d/out" "Fix (if no pacman is running): sudo rm $d/db.lck" lockfix
has "$d/out" '[f] fix and try again   [Enter] close' lockfix
[ ! -e "$d/db.lck" ] || fail "lockfix: the lock is still there"
[ -e "$d/pacman-ran" ] || fail "lockfix: the update did not run again"
has "$d/out" '✓ Done' lockfix
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

echo "pacman found the lock taken again: waited, then once more"
KYU_FAKE_MODE=lockedonce runtty relocked 'y\n|\n'
has "$work/relocked/out" 'Packages   sudo pacman -Syu (again)' relocked
has "$work/relocked/out" '✓ Done' relocked

echo "[f] removes the lock only if no program has it open (root checks)"
prepare lockinuse; touch "$work/lockinuse/db.lck"
exec 7<"$work/lockinuse/db.lck"
runtty lockinuse 'f\n|\n'
exec 7<&-
has "$work/lockinuse/out" "pacman's lock is in use by" lockinuse
[ -e "$work/lockinuse/db.lck" ] || fail "lockinuse: removed a lock in use"

echo "keys: [r] asks, then restarts (here: a fake systemctl)"
runtty reboot 'y\n|r\n|y\n'
[ -e "$work/reboot/systemctl-reboot" ] || fail "reboot: systemctl reboot did not run"
has "$work/reboot/out" 'Restart now? Unsaved work in open programs is lost. [y/N]' reboot

echo "keys: no [r] after half an update"
KYU_FAKE_MODE=midfail runtty halfway 'y\n|\n'
has "$work/halfway/out" 'Do not restart: the update is incomplete. Run it again first.' halfway
hasnt "$work/halfway/out" '[r] restart' halfway

echo "keys: [f] for the keyring, then the whole update again without the news"
prepare keyagain; state "$work/keyagain/state.json" 2 1 1
KYU_FAKE_MODE=keyring runtty keyagain 'y\n|y\n|f\n|y\n|\n'
d=$work/keyagain
[ -e "$d/keyring-fixed" ] || fail "keyagain: the keyring fix did not run"
[ "$(grep -c 'Continue? \[y/N\]' "$d/out")" = 1 ] || fail "keyagain: the news were asked again"
has "$d/out" 'The package lists are newer than the system: update before installing anything.' keyagain

echo "keyrings first: all installed ones, their own transaction, then -Su; not gnome-keyring"
prepare keyfirst; state "$work/keyfirst/state.json" 1 0 0 "gnome-keyring archlinux-keyring"
KYU_FAKE_KEYRING=new runtty keyfirst 'y\n|y\n|\n'
d=$work/keyfirst
has "$d/out" 'Keyrings   sudo pacman -Sy --needed archlinux-keyring manjaro-keyring' keyfirst
has "$d/out" 'Packages   sudo pacman -Su' keyfirst
[ "$(cat "$d/keyring-args" 2>/dev/null)" = "-Sy --needed archlinux-keyring manjaro-keyring" ] || fail "keyfirst: keyrings with $(cat "$d/keyring-args" 2>/dev/null)"
[ "$(cat "$d/pacman-args" 2>/dev/null)" = -Su ] || fail "keyfirst: then pacman $(cat "$d/pacman-args" 2>/dev/null)"
has "$d/out" ' · 4 packages' keyfirst
has "$d/out" '✓ Done' keyfirst
hasnt "$d/out" 'newer than the system' keyfirst

echo "keyrings first, refused: nothing else runs; the lists are synced"
prepare keyno; state "$work/keyno/state.json" 1 0 0 manjaro-keyring
KYU_FAKE_KEYRING=new runtty keyno 'n\n|\n'
d=$work/keyno
has "$d/out" 'Stopped. No packages were upgraded.' keyno
has "$d/out" 'The package lists are newer than the system: update before installing anything.' keyno
[ ! -e "$d/pacman-args" ] || fail "keyno: pacman $(cat "$d/pacman-args") ran after the keyrings were refused"

echo "keyrings first, then the rest refused: says what went in, and warns"
prepare keyrest; state "$work/keyrest/state.json" 1 0 0 manjaro-keyring
KYU_FAKE_KEYRING=new runtty keyrest 'y\n|n\n|\n'
d=$work/keyrest
has "$d/out" 'Stopped. Only archlinux-keyring, manjaro-keyring were upgraded.' keyrest
has "$d/out" 'The package lists are newer than the system: update before installing anything.' keyrest
grep -q '"transactionStarted": false' "$d/state/kyu/last-run.json" || fail "keyrest: last-run.json says a transaction was cut short"
grep -q '"changed": 2' "$d/state/kyu/last-run.json" || fail "keyrest: last-run.json without the keyrings"

echo "keyrings first, signatures still fail: no keyring fix offered again"
prepare keystill; state "$work/keystill/state.json" 1 0 0 archlinux-keyring
KYU_FAKE_KEYRING=new KYU_FAKE_MODE=keyring runtty keystill 'y\n|y\n|\n'
d=$work/keystill
has "$d/out" 'pacman failed. Only archlinux-keyring, manjaro-keyring were upgraded.' keystill
has "$d/out" 'Package signatures could not be checked.' keystill
has "$d/out" 'The keyrings are already current' keystill
has "$d/out" 'pacman-key --refresh-keys' keystill
hasnt "$d/out" 'Fix: ' keystill
hasnt "$d/out" '[f] fix' keystill

echo "the new keyring itself is not trusted (a long gap): no fix that cannot work"
prepare keyold; state "$work/keyold/state.json" 1 0 0 archlinux-keyring
KYU_FAKE_KEYRING=untrusted runtty keyold '\n'
d=$work/keyold
has "$d/out" 'No packages were upgraded.' keyold
has "$d/out" 'The new keyring itself could not be checked' keyold
has "$d/out" 'The package lists are newer than the system' keyold
hasnt "$d/out" 'Fix: ' keyold
[ ! -e "$d/pacman-args" ] || fail "keyold: pacman $(cat "$d/pacman-args") ran after the keyrings failed"

echo "keyrings first with --refresh: -Syy"
prepare keyrefresh; state "$work/keyrefresh/state.json" 1 0 0 archlinux-keyring
runtty keyrefresh 'y\n|\n' --refresh
d=$work/keyrefresh
[ "$(cat "$d/keyring-args" 2>/dev/null)" = "-Syy --needed archlinux-keyring manjaro-keyring" ] || fail "keyrefresh: keyrings with $(cat "$d/keyring-args" 2>/dev/null)"
[ "$(cat "$d/pacman-args" 2>/dev/null)" = -Su ] || fail "keyrefresh: then pacman $(cat "$d/pacman-args" 2>/dev/null)"

echo "keyrings first, then the lock taken: both again, the keyrings counted once"
prepare keylock; state "$work/keylock/state.json" 1 0 0 archlinux-keyring
KYU_FAKE_KEYRING=new KYU_FAKE_MODE=lockedonce runtty keylock 'y\n|y\n|\n'
d=$work/keylock
has "$d/out" 'Keyrings   sudo pacman -Sy --needed archlinux-keyring manjaro-keyring (again)' keylock
has "$d/out" ' · 4 packages' keylock
has "$d/out" '✓ Done' keylock

echo "after the keyring fix (--synced): no keyrings first"
prepare keysynced; state "$work/keysynced/state.json" 1 0 0 archlinux-keyring
runtty keysynced 'y\n|\n' --synced
d=$work/keysynced
hasnt "$d/out" 'Keyrings ' keysynced
[ "$(cat "$d/pacman-args" 2>/dev/null)" = -Syu ] || fail "keysynced: pacman ran with $(cat "$d/pacman-args" 2>/dev/null)"

echo "AUR: last, after pacman and Flatpaks, counted on its own"
prepare aur; state "$work/aur/state.json" 2 1 0 "" foo-bin
runtty aur 'y\n|y\n|\n' --aur-helper "$here/update/paru"
d=$work/aur
has "$d/out" 'Kyu · 4 updates: 2 packages, 1 AUR, 1 Flatpak' aur
has "$d/out" 'AUR        '"$here/update/paru"' -Sua' aur
[ "$(cat "$d/aur-args" 2>/dev/null)" = -Sua ] || fail "aur: paru ran with $(cat "$d/aur-args" 2>/dev/null)"
f=$(grep -n 'Flatpaks   flatpak update' "$d/out" | cut -d: -f1); a=$(grep -n ' -Sua' "$d/out" | head -1 | cut -d: -f1)
[ -n "$f" ] && [ -n "$a" ] && [ "$f" -lt "$a" ] || fail "aur: the AUR did not come after the Flatpaks"
has "$d/out" ' · 2 packages · 1 AUR · 1 Flatpak' aur

echo "AUR failed: said on its own line; the system update is done"
prepare aurfail; state "$work/aurfail/state.json" 2 0 0 "" foo-bin
KYU_FAKE_AUR=fail runtty aurfail 'y\n|y\n|\n' --aur-helper "$here/update/paru"
d=$work/aurfail
has "$d/out" '✓ Done' aurfail
has "$d/out" 'AUR packages not updated: paru stopped (status 1), see above. The rest is done.' aurfail
hasnt "$d/out" 'Nothing else to do' aurfail
grep -q '"aurExit": 1' "$d/state/kyu/last-run.json" || fail "aurfail: last-run.json without the AUR's status"
grep -q '"pacmanExit": 0' "$d/state/kyu/last-run.json" || fail "aurfail: the system update counted as failed"

grep -q '"name": "foo-bin"' "$d/state/kyu/last-run.json" || fail "aurfail: the failed AUR package is not remembered"
grep -q '"restartRequired": true' "$d/last-run-before-aur.json" 2>/dev/null || fail "aurfail: nothing saved before the AUR step"
has "$d/out" 'paru asks its own questions, and may ask for the password again.' aurfail
# The next update fails in pacman: the AUR package is still held back.
KYU_FAKE_MODE=keyring runtty aurfail 'y\n|\n' --aur-helper "$here/update/paru"
grep -q '"name": "foo-bin"' "$d/state/kyu/last-run.json" || fail "aurfail: the failed AUR package was forgotten by the next update"

echo "only AUR pending, and it fails: no \"up to date\", no \"the rest is done\""
prepare auronlyfail; state "$work/auronlyfail/state.json" 0 0 0 "" foo-bin
KYU_FAKE_AUR=fail runtty auronlyfail 'y\n|\n' --aur-helper "$here/update/paru"
d=$work/auronlyfail
has "$d/out" 'AUR packages not updated: paru stopped (status 1), see above.' auronlyfail
hasnt "$d/out" 'up to date' auronlyfail
hasnt "$d/out" 'The rest is done' auronlyfail
# Next time, the same version still listed: held back, not built again
# (seen in a real run with yay: it was offered again and again).
rm -f "$d/aur-args"
runtty auronlyfail '\n' --aur-helper "$here/update/paru"
has "$d/out" 'AUR, held back: foo-bin (not built last time; tried again when the AUR has a newer version).' auronlyfail-2
hasnt "$d/out" '1 AUR' auronlyfail-2
[ ! -e "$d/aur-args" ] || fail "auronlyfail-2: paru ran again for the package that failed"
grep -q '"name": "foo-bin"' "$d/state/kyu/last-run.json" || fail "auronlyfail-2: the held-back package was forgotten"

echo "a held-back AUR package stays held back when another one builds"
prepare aurheld; state "$work/aurheld/state.json" 0 0 0 "" "foo-bin bar-bin"
mkdir -p "$work/aurheld/state/kyu"
printf '{"v":1,"aurFailed":[{"name":"bar-bin","version":"2-1"}],"time":"2026-11-16T10:00:00Z"}' > "$work/aurheld/state/kyu/last-run.json"
runtty aurheld 'y\n|\n' --aur-helper "$here/update/paru"
d=$work/aurheld
grep -q -- '--ignore bar-bin' "$d/aur-args" || fail "aurheld: the helper was not told to leave bar-bin alone ($(cat "$d/aur-args" 2>/dev/null))"
grep -q '"name": "bar-bin"' "$d/state/kyu/last-run.json" || fail "aurheld: bar-bin forgotten after foo-bin built"
has "$d/out" 'AUR, held back: bar-bin' aurheld

echo "Flatpaks and AUR both failed: neither is called done"
prepare bothfail; state "$work/bothfail/state.json" 2 1 0 "" foo-bin
KYU_FAKE_AUR=fail KYU_FAKE_FLATPAK_EXIT=1 runtty bothfail 'y\n|y\n|\n' --aur-helper "$here/update/paru"
d=$work/bothfail
has "$d/out" ' · 2 packages' bothfail
has "$d/out" 'Flatpaks failed (status 1), see above.' bothfail
has "$d/out" 'AUR packages not updated: paru stopped (status 1), see above.' bothfail
hasnt "$d/out" 'The rest is done' bothfail

echo "Flatpaks failed, AUR done: both said"
prepare fpaur; state "$work/fpaur/state.json" 2 1 0 "" foo-bin
KYU_FAKE_FLATPAK_EXIT=1 runtty fpaur 'y\n|y\n|\n' --aur-helper "$here/update/paru"
d=$work/fpaur
has "$d/out" ' · 2 packages · 1 AUR' fpaur
has "$d/out" 'Flatpaks failed (status 1), see above.' fpaur

echo "pamac: pacman first even with only AUR pending (pamac would do the repos too)"
prepare pamac; state "$work/pamac/state.json" 0 0 0 "" foo-bin
runtty pamac 'y\n|\n' --aur-helper "$here/update/pamac"
d=$work/pamac
[ -e "$d/pacman-ran" ] || fail "pamac: pacman was skipped"
[ "$(cat "$d/aur-args" 2>/dev/null)" = "update --aur" ] || fail "pamac: ran with $(cat "$d/aur-args" 2>/dev/null)"

echo "AUR skipped when pacman failed"
prepare aurskip; state "$work/aurskip/state.json" 2 1 0 "" foo-bin
KYU_FAKE_MODE=keyring runtty aurskip 'y\n|\n' --aur-helper "$here/update/paru"
d=$work/aurskip
has "$d/out" 'Flatpaks and AUR packages were skipped.' aurskip
[ ! -e "$d/aur-args" ] || fail "aurskip: the AUR helper ran after pacman failed"

echo "only AUR packages pending: no pacman, the helper alone"
prepare auronly; state "$work/auronly/state.json" 0 0 0 "" foo-bin
runtty auronly 'y\n|\n' --aur-helper "$here/update/paru"
d=$work/auronly
[ ! -e "$d/pacman-ran" ] || fail "auronly: pacman ran"
[ -e "$d/aur-args" ] || fail "auronly: the AUR helper did not run"
has "$d/out" ' · 1 AUR' auronly

echo "a restart from before this run says so"
prepare earlier; state "$work/earlier/state.json" 0 0 0
KYU_FAKE_MODE=nothing run earlier ''
has "$work/earlier/out" 'Restart required: new kernel 6.18.22-1-MANJARO (from an earlier update)' earlier

echo "--merge: pacdiff only, then the record keeps what is left"
run mergeonly 'y\n'
d=$work/mergeonly
: >"$d/pacdiff-ran.before"
rm -f "$d/pacdiff-ran"
run mergeonly '' --merge
has "$d/out" 'Kyu · configuration files' mergeonly
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

echo "housekeeping: after a whole update, what can go is listed, [c] cleans each part with its own question"
KYU_FAKE_ORPHANS="gtk2 libfoo base" KYU_FAKE_ORPHAN_DEPS="libbar" KYU_FAKE_CACHE=big KYU_TEST_UNUSED_FLATPAKS="org.kde.Platform//6.8" runtty clean 'y\n|c\n|y\n|y\n|y\n|\n'
d=$work/clean
has "$d/out" 'Can be cleaned up, if you like:' clean
# base is never offered, whatever its install reason.
has "$d/out" '2 orphan packages: gtk2, libfoo (3 packages in all, with what only they need)' clean
has "$d/out" 'of older and removed packages in the package cache' clean
has "$d/out" '1 Flatpak runtime no app uses' clean
has "$d/out" '[c] clean up' clean
[ "$(cat "$d/remove-args" 2>/dev/null)" = '-Rs gtk2 libfoo' ] || fail "clean: pacman got '$(cat "$d/remove-args" 2>/dev/null)'"
[ "$(cat "$d/paccache-args" 2>/dev/null | tr '\n' ' ')" = '-rk3 -ruk0 ' ] || fail "clean: paccache got '$(cat "$d/paccache-args" 2>/dev/null)'"
[ "$(cat "$d/flatpak-unused-args" 2>/dev/null)" = 'uninstall --unused' ] || fail "clean: flatpak got '$(cat "$d/flatpak-unused-args" 2>/dev/null)'"
has "$d/out" 'sudo pacman -D --asexplicit NAME' clean
# The fake runtime stays listed (the test's answer does not change): the rest is gone.
hasnt "$d/out" 'Orphans not removed' clean

echo "housekeeping: orphans and the cache answered n are kept, said plainly, and not offered again in this run"
KYU_FAKE_ORPHANS="gtk2" KYU_FAKE_CACHE=big runtty cleanno 'y\n|c\n|n\n|n\n|\n'
d=$work/cleanno
has "$d/out" 'Orphans kept.' cleanno
hasnt "$d/out" 'not removed' cleanno
[ ! -e "$d/paccache-args" ] || fail "cleanno: paccache ran"
[ "$(grep -c '\[c\] clean up' "$d/out")" = 1 ] || fail "cleanno: [c] offered again after both were declined"

echo "housekeeping: Ctrl+C at the cache question stops the rest of the clean-up"
KYU_FAKE_CACHE=big KYU_TEST_UNUSED_FLATPAKS="org.kde.Platform//6.8" runtty cleanctrlc 'y\n|c\n|\003|\n'
d=$work/cleanctrlc
[ ! -e "$d/paccache-args" ] || fail "cleanctrlc: paccache ran"
[ ! -e "$d/flatpak-unused-args" ] || fail "cleanctrlc: flatpak ran after Ctrl+C"

echo "housekeeping: no orphans offered when pacman's dry run fails; -R alone when -Rs would take a protected package"
KYU_FAKE_ORPHANS="gtk2" KYU_FAKE_DRYRUN=fail runtty cleandry 'y\n|\n'
hasnt "$work/cleandry/out" 'orphan package' cleandry
KYU_FAKE_ORPHANS="gtk2" KYU_FAKE_ORPHAN_DEPS="linux-firmware-whence" runtty cleanprot 'y\n|c\n|y\n|\n'
d=$work/cleanprot
has "$d/out" '1 orphan package: gtk2' cleanprot
hasnt "$d/out" 'in all, with what only they need' cleanprot
[ "$(cat "$d/remove-args" 2>/dev/null)" = '-R gtk2' ] || fail "cleanprot: pacman got '$(cat "$d/remove-args" 2>/dev/null)'"

echo "housekeeping: nothing shown after a failed update, nor when there is nothing"
KYU_FAKE_ORPHANS="gtk2" KYU_FAKE_MODE=midfail runtty cleanfail 'y\n|\n'
hasnt "$work/cleanfail/out" 'Can be cleaned up' cleanfail
hasnt "$work/cleanfail/out" '[c] clean up' cleanfail
runtty cleannone 'y\n|\n'
hasnt "$work/cleannone/out" 'Can be cleaned up' cleannone

echo "housekeeping: kyu-update --clean on its own"
KYU_FAKE_CACHE=big runtty cleanonly 'c\n|y\n|\n' --clean
d=$work/cleanonly
has "$d/out" 'Kyu · clean up' cleanonly
[ "$(cat "$d/paccache-args" 2>/dev/null | tr '\n' ' ')" = '-rk3 -ruk0 ' ] || fail "cleanonly: paccache got '$(cat "$d/paccache-args" 2>/dev/null)'"
has "$d/out" 'Cleaned up.' cleanonly
[ ! -e "$d/pacman-ran" ] || fail "cleanonly: the update ran"
runtty cleanonlynone '\n' --clean
has "$work/cleanonlynone/out" 'Nothing to clean up.' cleanonlynone

if [ -e "$qm/kyu_de.qm" ]; then
    echo "Kyu's own words in German; last-run.json stays English (the card reads it)"
    KYU_TEST_NO_LC_ALL=1 LC_MESSAGES= LANG=de_DE.UTF-8 LANGUAGE=de KYU_TRANSLATIONS_DIR="$qm" run deok 'y\n'
    d=$work/deok
    [ "$(cat "$d/code")" = 0 ] || fail "deok: exit $(cat "$d/code")"
    has "$d/out" '3 Aktualisierungen: 2 Pakete, 1 Flatpak' deok
    has "$d/out" 'Dieses Fenster geöffnet lassen, bis Kyu meldet, dass alles erledigt ist.' deok
    has "$d/out" 'Pakete     sudo pacman -Syu' deok
    has "$d/out" ' 2 Pakete ' deok
    has "$d/out" 'Fertig' deok
    has "$d/out" 'Neustart erforderlich: neuer Kernel 6.18.22-1-MANJARO' deok
    has "$d/out" '1 Konfigurationsdatei zusammenzuführen: /etc/sddm.conf' deok
    hasnt "$d/out" 'Restart required' deok
    grep -q '"restartReason": "new kernel 6.18.22-1-MANJARO"' "$d/state/kyu/last-run.json" || fail "deok: last-run.json's reason not in English"
    echo "German: the keys and y/N stay as they are, and work"
    LC_ALL=de_DE.UTF-8 KYU_TRANSLATIONS_DIR="$qm" KYU_FAKE_CACHE=big runtty declean 'c\n|y\n|\n' --clean
    d=$work/declean
    has "$d/out" ' Aufräumen' declean
    has "$d/out" '[c] aufräumen   [Enter] schließen' declean
    has "$d/out" 'nicht mehr installierte Pakete? [y/N]' declean
    [ "$(cat "$d/paccache-args" 2>/dev/null | tr '\n' ' ')" = '-rk3 -ruk0 ' ] || fail "declean: paccache got '$(cat "$d/paccache-args" 2>/dev/null)'"
    has "$d/out" 'Aufgeräumt.' declean
fi
echo "English first in LANGUAGE, or the C locale whatever LANGUAGE says: English"
KYU_TEST_NO_LC_ALL=1 LC_MESSAGES= LANG=de_DE.UTF-8 LANGUAGE=en_US:de run deen 'y\n'
has "$work/deen/out" 'Restart required: new kernel 6.18.22-1-MANJARO' deen
LANGUAGE=de run cde 'y\n'
has "$work/cde/out" 'Restart required: new kernel 6.18.22-1-MANJARO' cde

[ $fails -eq 0 ] && echo "all kyu-update tests passed"
[ $fails -eq 0 ]
