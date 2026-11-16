#!/bin/bash
# Screenshots of kyu-update in real Konsole windows: every outcome, in your
# own Konsole profile, in Konsole's bundled colour schemes, and narrow/wide.
# Each window runs the real kyu-update against a fake system (fake sudo,
# pacman and flatpak first on PATH, fake pacman.log, /proc and lock file in a
# temporary directory): nothing on your machine changes, no root, no network.
# Answers are typed into the window over Konsole's D-Bus interface.
#
#   tools/kyu-update-screens.sh OUTDIR
#
# Needs konsole, spectacle and busctl. Takes about 4 minutes; windows open
# and close by themselves: leave the desktop alone (the screenshot is of the
# active window). Environment: BUILD (default ./build), SCHEMES (Konsole
# colour schemes, default: those bundled with Konsole that are installed),
# ONLY (a shot name prefix, for re-running a few).
set -u
src=$(cd "$(dirname "$0")/.." && pwd)
build=$(realpath -m "${BUILD:-$src/build}")
out=$(realpath -m "${1:?usage: kyu-update-screens.sh OUTDIR}")
fakes=$src/tests/update
. "$fakes/fake-system.sh"

for tool in konsole spectacle busctl script; do
    command -v $tool >/dev/null || { echo "kyu-update-screens: $tool is not installed" >&2; exit 2; }
done
[ -x "$build/bin/kyu-update" ] || { echo "kyu-update-screens: $build/bin/kyu-update is missing; build first" >&2; exit 2; }

# The binary is copied so window titles and messages show no checkout path.
work=$(mktemp -d "${TMPDIR:-/tmp}/kyu-screens.XXXXXX")
pid=
cleanup() {
    [ -n "$pid" ] && kill "$pid" 2>/dev/null
    rm -rf "$work"
}
trap cleanup EXIT
trap 'exit 130' INT TERM
mkdir -p "$out" "$work/bin"
cp "$build/bin/kyu-update" "$work/bin/"
log() { echo "$*" | tee -a "$out/screens.log"; }
failed=0

# shot NAME SCHEME SIZE MODE STATE STEPS...
#   SCHEME: "-" for your own profile's, else a Konsole colour scheme name.
#   SIZE: normal | narrow | wide.  MODE: the fake pacman's KYU_FAKE_MODE.
#   STATE: "packages flatpaks news(0/1)", or "-" for 2 1 0.
#   FLATPAK_EXIT=N before the call makes the fake flatpak fail.
#   STEPS: "wait SECONDS", "type TEXT" (then Enter), "enter", "lock",
#          "busy" (a running pacman holds the lock), "shot", "shot2".
shot() {
    local name=$1 scheme=$2 size=$3 mode=$4 st=$5
    shift 5
    case "$name" in "${ONLY:-}"*) ;; *) return ;; esac
    local d="$work/$name" geometry
    rm -rf "$d"
    mkdir -p "$d"
    [ "$st" = - ] || fake_state "$d/state.json" $st
    fake_system "$d"
    case "$size" in
    narrow) geometry=560x560 ;;
    wide) geometry=1300x700 ;;
    *) geometry=900x620 ;;
    esac
    local -a profile=(-p "LocalTabTitleFormat=Kyu test" -p "RemoteTabTitleFormat=Kyu test")
    [ "$scheme" = - ] || profile+=(-p "ColorScheme=$scheme")
    local step arg
    # Steps that set up the fake system come before the window opens.
    for step in "$@"; do
        case "$step" in
        lock) touch "$d/db.lck" ;;
        busy) touch "$d/db.lck"; mkdir -p "$d/proc/4242"; echo pacman >"$d/proc/4242/comm" ;;
        esac
    done
    # shellcheck disable=SC2046 # fake_args is a word list without spaces
    env PATH="$fakes:$PATH" KYU_FAKE_DIR="$d" KYU_FAKE_MODE="$mode" KYU_FAKE_FLATPAK_EXIT="${FLATPAK_EXIT:-0}" XDG_STATE_HOME="$d/state" \
        konsole --separate --hide-menubar --hide-tabbar --workdir "$d" -qwindowgeometry "$geometry" "${profile[@]}" \
        -e "$work/bin/kyu-update" $(fake_args "$d") >>"$work/konsole.log" 2>&1 &
    pid=$!
    local service=org.kde.konsole-$pid i
    for i in $(seq 50); do
        busctl --user status "$service" >/dev/null 2>&1 && break
        sleep 0.2
    done
    if ! busctl --user status "$service" >/dev/null 2>&1; then
        log "FAILED $name: Konsole did not appear on D-Bus"
        failed=$((failed + 1))
        kill "$pid" 2>/dev/null; pid=
        return
    fi
    sleep 1.5 # the window maps, kyu-update starts
    local n=0
    for step in "$@"; do
        arg=${step#* }
        case "$step" in
        wait\ *) sleep "$arg" ;;
        type\ *) busctl --user call "$service" /Sessions/1 org.kde.konsole.Session sendText s "$arg"$'\r' >/dev/null; sleep 0.8 ;;
        enter) busctl --user call "$service" /Sessions/1 org.kde.konsole.Session sendText s $'\r' >/dev/null; sleep 0.8 ;;
        shot)
            n=$((n + 1))
            local label=$name
            [ $n -gt 1 ] && label="$name-$n"
            local file="$out/$label.png"
            rm -f "$file"
            spectacle --background --nonotify --activewindow --output "$file" >>"$work/konsole.log" 2>&1
            for i in $(seq 25); do [ -s "$file" ] && break; sleep 0.2; done
            if [ -s "$file" ]; then log "ok $label"; else log "FAILED $name: no screenshot"; failed=$((failed + 1)); fi
            ;;
        esac
    done
    # Close as a person would: Enter at "Press Enter to close" ends kyu-update
    # and Konsole closes with it. (A second Enter if the first came early,
    # e.g. at "Continue? [y/N]".)
    local try
    for try in 1 2; do
        busctl --user call "$service" /Sessions/1 org.kde.konsole.Session sendText s $'\r' >/dev/null 2>&1
        for i in $(seq 15); do kill -0 "$pid" 2>/dev/null || break; sleep 0.2; done
        kill -0 "$pid" 2>/dev/null || break
    done
    if kill -0 "$pid" 2>/dev/null; then
        log "note $name: the window did not close after Enter"
        kill "$pid" 2>/dev/null
    fi
    wait "$pid" 2>/dev/null
    pid=
    # What kyu-update left for the card.
    [ -f "$d/state/kyu/last-run.json" ] && cp "$d/state/kyu/last-run.json" "$out/$name.last-run.json"
}

# All outcomes, in your own profile.
outcomes() { # PREFIX SCHEME SIZE
    local p=$1 s=$2 z=$3
    shot "$p-01-done" "$s" "$z" normal - "type y" shot
    shot "$p-02-news-question" "$s" "$z" normal "2 1 1" shot "type y" "type y" shot
    shot "$p-03-news-stopped" "$s" "$z" normal "2 1 1" enter shot
    shot "$p-04-downloading" "$s" "$z" slow - "type y" wait 1 shot wait 4 shot
    shot "$p-05-keyring" "$s" "$z" keyring - "type y" shot
    shot "$p-06-declined" "$s" "$z" normal - "type n" shot
    shot "$p-07-failed-midway" "$s" "$z" midfail - "type y" shot
    shot "$p-08-other-language" "$s" "$z" german - shot
    shot "$p-09-stale-lock" "$s" "$z" normal - lock shot
    shot "$p-10-busy" "$s" "$z" normal - busy shot
    shot "$p-11-flatpak-only" "$s" "$z" normal "0 2 0" shot
    FLATPAK_EXIT=1 shot "$p-12-flatpak-failed" "$s" "$z" normal - "type y" shot
    shot "$p-13-nothing-new" "$s" "$z" nothing - shot
}

log "== your Konsole profile"
outcomes A - normal

log "== Konsole's colour schemes"
schemes=${SCHEMES:-}
if [ -z "$schemes" ]; then
    for s in Breeze BlackOnWhite BlackOnLightYellow Solarized SolarizedLight DarkPastels WhiteOnBlack Linux GreenOnBlack; do
        [ -f "/usr/share/konsole/$s.colorscheme" ] && schemes="$schemes $s"
    done
fi
for s in $schemes; do
    shot "B-$s-done" "$s" normal normal - "type y" shot
    shot "B-$s-keyring" "$s" normal keyring - "type y" shot
    shot "B-$s-news" "$s" normal normal "2 1 1" shot
done

log "== narrow and wide windows"
for z in narrow wide; do
    shot "C-$z-done" - "$z" normal - "type y" shot
    shot "C-$z-keyring" - "$z" keyring - "type y" shot
    shot "C-$z-news" - "$z" normal "2 1 1" shot
done

cp "$work/konsole.log" "$out/konsole.log" 2>/dev/null
log "done: $(ls "$out"/*.png 2>/dev/null | wc -l) screenshots, $failed failed"
[ $failed -eq 0 ]
