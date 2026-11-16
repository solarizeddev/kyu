#!/bin/sh
# Runs the real panel against a simulated system: a test kyu-helper on a
# private session bus, a fake pacman, a throwaway Flatpak installation with a
# local repo, a fake checker, and a fake /proc and kernel modules directory.
# Nothing touches your packages, Flatpaks, pacman database or running Kyu.
#
#   tools/kyu-demo.sh SCENARIO [kyu options...]    open the panel; use it
#   OUT=shot.png tools/kyu-demo.sh SCENARIO ...    screenshot after a delay, quit
#   tools/kyu-demo.sh --list                       list the scenarios
#
# Environment (all optional):
#   SCHEME=BreezeDark      colour scheme by name (~/.local/share/color-schemes,
#                          /usr/share/color-schemes) or path to a .colors file;
#                          default: your current one
#   FONT_PT=14             general font size in points (fixed-width follows)
#   SCALE=2                QT_SCALE_FACTOR (HiDPI)
#   SIZE=300x420           window size in logical pixels; default: Kyu's own
#   OFFSCREEN=1            render without a window (always for preview builds
#                          and without a display)
#   SHOT_DELAY=MS          override the scenario's screenshot delay
#   BUILD=dir              build directory (default ./build)
#
# Theme, font and scale apply only to the panel: they use a private copy of
# your kdeglobals and do not change your desktop.
set -eu
src=$(cd "$(dirname "$0")/.." && pwd)
demo="$src/tools/demo"

scenarios='
list            the update list: 8 packages, 2 Flatpaks, Arch news, a kernel restart
list-long       154 updates, long names and versions, a failed check, long news title
list-empty      nothing to update
list-flatpak    Flatpak updates only
list-manjaro    Manjaro: the branch announcement instead of Arch news
check-failed    the last check failed (no network): the list is from earlier
stale-lock      a pacman lock file nobody holds (offers to remove it)
interrupted     an earlier update ended without a result
running         an update in progress (slow fake pacman)
q-provider      pacman asks which provider to install
q-conflict      pacman asks to remove a conflicting package
q-replace       pacman asks to replace a package
q-key           pacman asks to import a PGP key
q-skip          pacman cannot resolve packages (only Cancel)
q-unknown       pacman asks something Kyu does not know: stopped safely
mirror-changed  pacman upgrades more than the list showed (mirror changed since the check)
cancelled       a conflict question answered with Cancel
answered        a provider question answered (option 2), then finished
nothing         pacman finds nothing to do
finished        success: restart required, a .pacnew file, programs using old libraries
fail-network    failed: no network
fail-mirror     failed: out-of-date mirror (404)
fail-keyring    failed: invalid signature / keyring
fail-files      failed: files already on disk
fail-disk       failed: not enough disk space
fail-deps       failed: dependency error
page-output     finished, full output page
page-changes    finished, what changed page
page-configs    finished, settings files page
page-diff       finished, diff of a settings file
page-processes  finished, programs using replaced libraries
'
if [ "${1:-}" = --list ] || [ $# -eq 0 ]; then
    printf '%s\n' "$scenarios" | sed '/^$/d'
    [ $# -eq 0 ] && { echo "usage: tools/kyu-demo.sh SCENARIO [kyu options...]" >&2; exit 2; }
    exit 0
fi
scenario=$1; shift
printf '%s\n' "$scenarios" | grep -q "^$scenario " || { echo "kyu-demo: unknown scenario '$scenario' (tools/kyu-demo.sh --list)" >&2; exit 2; }

# Defaults, then per scenario: state file, fake pacman mode and speed, kyu
# options, the state the fake checker writes afterwards, screenshot delay.
state=state.json; mode=normal; delay=1; after=; shot=1500; stale=; interrupted=
# What the fake pacman upgrades: the pacman packages of state.json.
packages="linux618 linux618-headers kwin mesa plasma-desktop plasma-workspace"
set -- "$@"
args=
case "$scenario" in
    list) ;;
    list-long) state=state-long.json ;;
    list-empty) state=state-empty.json ;;
    list-flatpak) state=state-flatpak.json ;;
    list-manjaro) state=state-manjaro.json ;;
    check-failed) state=state-check-failed.json ;;
    stale-lock) stale=1 ;;
    interrupted) interrupted=1 ;;
    running) delay=1.5; args="--start-update"; shot=2500 ;;
    q-provider|q-conflict|q-replace|q-key|q-skip|q-unknown) mode=${scenario#q-}; args="--start-update"; shot=3000 ;;
    mirror-changed) delay=1.5; packages="$packages glibc libxml2"; args="--start-update"; shot=3000 ;;
    cancelled) mode=conflict; args="--start-update --answer cancel"; shot=4500 ;;
    answered) mode=provider; delay=0.05; args="--start-update --answer 2"; after=state-empty.json; shot=8000 ;;
    nothing) mode=nothing; args="--start-update"; after=state-empty.json; shot=3000 ;;
    finished) delay=0.05; args="--start-update"; after=state-empty.json; shot=7000 ;;
    fail-network|fail-mirror|fail-keyring|fail-files|fail-disk) mode=${scenario#fail-}; args="--start-update"; shot=3500 ;;
    fail-deps) mode=fail; args="--start-update"; shot=3500 ;;
    page-*) delay=0.05; args="--start-update --page ${scenario#page-} --page-delay 7000"; after=state-empty.json; shot=8000 ;;
esac
shot=${SHOT_DELAY:-$shot}

bin=$(realpath "${BUILD:-$src/build}")/bin
[ -x "$bin/kyu" ] && [ -x "$bin/kyu-helper" ] || { echo "kyu-demo: build first (cmake --build ${BUILD:-build})" >&2; exit 1; }
command -v dbus-run-session >/dev/null || { echo "kyu-demo: needs dbus-run-session (package dbus)" >&2; exit 1; }
command -v flatpak >/dev/null || { echo "kyu-demo: needs flatpak" >&2; exit 1; }
preview=
QT_QPA_PLATFORM=offscreen "$bin/kyu" --help 2>/dev/null | grep -q -- '--dark' && preview=1

work=$(mktemp -d "${TMPDIR:-/tmp}/kyu-demo.XXXXXX")
trap 'rm -rf "$work"' EXIT INT TERM

# --- Simulated system -----------------------------------------------------
mkdir -p "$work/state/kyu" "$work/fake"
export FLATPAK_USER_DIR="$work/flatpak-user" FLATPAK_SYSTEM_DIR="$work/flatpak-system"
. "$src/tests/flatpak/make-repo.sh"
make_repo "$work" flathub
case "$state" in state-empty.json) ;; *)
    publish "$work" org.kyu.Test.App 2
    publish "$work" "$KYU_TEST_RUNTIME" 2 ;;
esac
cp "$demo/$state" "$work/state/kyu/updates.json"
cp "$demo/${after:-$state}" "$work/after.json"
cp "$src/tests/helper/fake-pacman" "$work/fake/pacman"
echo "$mode" >"$work/fake/mode"
echo "$delay" >"$work/fake/delay"
echo "$packages" >"$work/fake/packages"
# Restart facts: the running kernel 6.18.20 was replaced by 6.18.22, and
# plasmashell maps a replaced library.
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
[ -n "$stale" ] && touch "$work/db.lck"
if [ -n "$interrupted" ]; then
    mkdir -p "$work/logs"
    printf '{"v":1,"tx":"20261116T101723Z-00","seq":1,"time":"2026-11-16T10:17:23Z","kind":"started","uid":1000}\n' \
        >"$work/logs/20261116T101723Z-00.jsonl"
fi

# --- Theme: a private kdeglobals ------------------------------------------
# (The preview build's stand-in has Breeze colours built in: --dark, --font-size.)
if [ -z "$preview" ] && [ -n "${SCHEME:-}${FONT_PT:-}" ]; then
    conf="$work/config"
    mkdir -p "$conf"
    userconf=${XDG_CONFIG_HOME:-$HOME/.config}
    # Everything else stays your own (style settings, icons), read through links.
    for f in "$userconf"/* "$userconf"/.[!.]*; do
        [ -e "$f" ] && [ "$(basename "$f")" != kdeglobals ] && ln -s "$f" "$conf/"
    done
    colors=
    if [ -n "${SCHEME:-}" ]; then
        for c in "$SCHEME" "$HOME/.local/share/color-schemes/$SCHEME.colors" "/usr/share/color-schemes/$SCHEME.colors"; do
            [ -f "$c" ] && { colors=$c; break; }
        done
        [ -n "$colors" ] || { echo "kyu-demo: colour scheme '$SCHEME' not found; installed:" >&2;
            ls /usr/share/color-schemes "$HOME/.local/share/color-schemes" 2>/dev/null | sed -n 's/\.colors$//p' >&2; exit 1; }
    fi
    # Your kdeglobals without the parts being replaced, then the scheme, then fonts.
    awk -v drop_colors="${colors:+1}" -v drop_fonts="${FONT_PT:+1}" '
        /^\[/ { group=$0; skip = drop_colors && (group ~ /^\[(Colors:|ColorEffects:|WM\])/) }
        skip { next }
        group == "[General]" && drop_colors && /^(ColorScheme|Name)=/ { next }
        group == "[General]" && drop_fonts && /^(font|fixed|smallestReadableFont|toolBarFont|menuFont)=/ { next }
        group == "[WM]" && drop_fonts && /^activeFont=/ { next }
        { print }' "$userconf/kdeglobals" 2>/dev/null >"$conf/kdeglobals" || true
    [ -n "$colors" ] && { echo; cat "$colors"; } >>"$conf/kdeglobals"
    if [ -n "${FONT_PT:-}" ]; then
        small=$(awk -v p="$FONT_PT" 'BEGIN { printf "%d", (p * 0.8 < 6 ? 6 : p * 0.8) }')
        sans=$(fc-match -f '%{family[0]}' sans-serif 2>/dev/null || echo "Noto Sans")
        mono=$(fc-match -f '%{family[0]}' monospace 2>/dev/null || echo "Hack")
        printf '\n[General]\nfont=%s,%s,-1,5,400,0,0,0,0,0,0,0,0,0,0,1\nfixed=%s,%s,-1,5,400,0,0,0,0,0,0,0,0,0,0,1\nsmallestReadableFont=%s,%s,-1,5,400,0,0,0,0,0,0,0,0,0,0,1\n' \
            "$sans" "$FONT_PT" "$mono" "$FONT_PT" "$sans" "$small" >>"$conf/kdeglobals"
    fi
    export XDG_CONFIG_HOME="$conf"
fi
[ -n "${SCALE:-}" ] && export QT_SCALE_FACTOR="$SCALE"
if [ -n "$preview" ] || [ "${OFFSCREEN:-}" = 1 ] || [ -z "${WAYLAND_DISPLAY:-}${DISPLAY:-}" ]; then
    export QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
    [ -z "$preview" ] && export QT_QPA_PLATFORMTHEME="${QT_QPA_PLATFORMTHEME:-kde}"
fi
[ -n "$preview" ] && case "${SCHEME:-}" in *[Dd]ark*) args="$args --dark" ;; esac
[ -n "$preview" ] && [ -n "${FONT_PT:-}" ] && args="$args --font-size $FONT_PT"
[ -n "${SIZE:-}" ] && args="$args --size $SIZE"
out=
[ -n "${OUT:-}" ] && { out=$(realpath -m "$OUT"); mkdir -p "$(dirname "$out")"; args="$args --screenshot $out --delay $shot"; }

# --- Run ------------------------------------------------------------------
export XDG_STATE_HOME="$work/state" XDG_CACHE_HOME="$work/cache" KYU_NO_SYSTEMD=1 \
    KYU_CHECK="$demo/fake-kyu-check" KYU_DEMO_AFTER="$work/after.json"
# shellcheck disable=SC2086 # $args is a list of words
dbus-run-session -- sh -c '
    work=$1; bin=$2; out=$3; shift 3
    "$bin/kyu-helper" --session --pacman "$work/fake/pacman" --db-lock "$work/db.lck" --log-dir "$work/logs" \
        --proc-dir "$work/proc" --modules-dir "$work/modules" 2>"$work/helper.err" &
    helper=$!
    sleep 0.5
    if [ -n "$out" ]; then timeout 90 "$bin/kyu" --session-bus "$@"; else "$bin/kyu" --session-bus "$@"; fi
    code=$?
    # SIGKILL: on SIGTERM the helper waits for a running update to finish.
    kill -9 "$helper" 2>/dev/null
    exit $code
' sh "$work" "$bin" "$out" $args "$@" 2>&1 | grep -v -e 'fd limit' -e XDG_RUNTIME_DIR -e propagateSizeHints -e '^kyu-flatpak: ' || true
if [ -n "$out" ]; then
    [ -f "$out" ] && echo "wrote $out" || { echo "kyu-demo: no screenshot for $scenario" >&2; exit 1; }
fi
