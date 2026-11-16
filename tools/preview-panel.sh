#!/bin/sh
# Screenshots the panel without KF6 or root, for development.
#
# Needs a build configured with -DKYU_PREVIEW=ON (Kirigami stand-in) and the
# flatpak command. Runs kyu-helper on a private session bus with
# tests/helper/fake-pacman, a real local Flatpak repository (remote named
# "flathub", see tests/flatpak/make-repo.sh) with two pending updates, and the
# sample update list in tools/preview-state.json.
#
#   tools/preview-panel.sh OUT.png [kyu preview options...]
#
# Examples:
#   tools/preview-panel.sh list.png
#   tools/preview-panel.sh list-dark.png --dark
#   tools/preview-panel.sh running.png --start-update --delay 3300
#   tools/preview-panel.sh output.png --start-update --page output --page-delay 2000 --delay 3300
#   MODE=provider tools/preview-panel.sh question.png --start-update --delay 1800
#   MODE=provider tools/preview-panel.sh answered.png --start-update --answer 3 --delay 4000
#   MODE=conflict tools/preview-panel.sh cancelled.png --start-update --answer cancel --delay 3000
#
# MODE (see tests/helper/fake-pacman) and DELAY (seconds per package) set the
# fake pacman's behaviour. STALE_LOCK=1 leaves a db.lck that no process holds;
# INTERRUPTED=1 seeds a helper log that ends without a result. BUILD defaults
# to ./build.
set -eu
out=$(realpath -m "$1"); shift
src=$(cd "$(dirname "$0")/.." && pwd)
bin=$(realpath "${BUILD:-$src/build}")/bin
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

mkdir -p "$work/state/kyu" "$work/fake"
export FLATPAK_USER_DIR="$work/flatpak-user" FLATPAK_SYSTEM_DIR="$work/flatpak-system"
. "$src/tests/flatpak/make-repo.sh"
make_repo "$work" flathub
publish "$work" org.kyu.Test.App 2
publish "$work" "$KYU_TEST_RUNTIME" 2
cp "$src/tools/preview-state.json" "$work/state/kyu/updates.json"
cp "$src/tests/helper/fake-pacman" "$work/fake/pacman"
# Restart facts: the running kernel 6.18.20 was replaced by 6.18.22, and
# plasmashell maps a replaced library (a fake /proc and /usr/lib/modules).
mkdir -p "$work/proc/sys/kernel" "$work/proc/1301" "$work/modules/6.18.22-1-MANJARO"
echo 6.18.20-1-MANJARO >"$work/proc/sys/kernel/osrelease"
echo "BOOT_IMAGE=/boot/vmlinuz-linux618 rw" >"$work/proc/cmdline"
echo plasmashell >"$work/proc/1301/comm"
echo "7f00-7f01 r--p 0 00:1f 9  /usr/lib/libQt6Core.so.6.10.1 (deleted)" >"$work/proc/1301/maps"
touch "$work/modules/6.18.22-1-MANJARO/vmlinuz"
echo linux618 >"$work/modules/6.18.22-1-MANJARO/pkgbase"
# A settings file and its .pacnew, for the diff view.
mkdir -p "$work/etc"
printf '[General]\nNumlock=on\n\n[Theme]\nCurrent=breeze\n' >"$work/etc/sddm.conf"
printf '[General]\nNumlock=none\nHaltCommand=/usr/bin/systemctl poweroff\n\n[Theme]\nCurrent=breeze\n' >"$work/etc/sddm.conf.pacnew"
echo "$work/etc/sddm.conf" >"$work/fake/pacnew"
[ "${STALE_LOCK:-}" = 1 ] && touch "$work/db.lck"
if [ "${INTERRUPTED:-}" = 1 ]; then
    mkdir -p "$work/logs"
    printf '{"v":1,"tx":"20261116T101723Z-00","seq":1,"time":"2026-11-16T10:17:23Z","kind":"started","uid":1000}\n' \
        >"$work/logs/20261116T101723Z-00.jsonl"
fi
echo "${MODE:-normal}" >"$work/fake/mode"
echo "${DELAY:-1}" >"$work/fake/delay"

dbus-run-session -- sh -c '
    work=$1; bin=$2; out=$3; shift 3
    "$bin/kyu-helper" --session --pacman "$work/fake/pacman" --db-lock "$work/db.lck" --log-dir "$work/logs" \
        --proc-dir "$work/proc" --modules-dir "$work/modules" 2>/dev/null &
    helper=$!
    sleep 0.5
    # After an update the panel runs kyu-check, which rewrites the sample
    # state in $work (without pacman it records a failed check).
    XDG_STATE_HOME="$work/state" XDG_CACHE_HOME="$work/cache" \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
        timeout 60 "$bin/kyu" --session-bus --screenshot "$out" "$@"
    code=$?
    # SIGKILL: on SIGTERM the helper waits for a running update to finish.
    kill -9 "$helper"
    exit $code
' sh "$work" "$bin" "$out" "$@" 2>&1 | grep -v -e 'fd limit' -e XDG_RUNTIME_DIR -e propagateSizeHints -e 'kyu-check' -e 'updates:' || true
[ -f "$out" ] && echo "wrote $out"
