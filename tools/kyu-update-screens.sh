#!/bin/bash
# Screenshots of kyu-update in real Konsole windows: every outcome, in your
# own Konsole profile, in Konsole's bundled colour schemes, narrow and wide.
# Each window runs the real kyu-update against a fake system (fake sudo,
# pacman and flatpak first on PATH, fake pacman.log, /proc and lock file in a
# temporary directory): nothing on your machine changes, no root, no network.
# Answers are typed into the window over Konsole's D-Bus interface.
#
#   tools/kyu-update-screens.sh OUTDIR
#
# Needs konsole, spectacle and busctl. Takes about 5 minutes; windows open
# and close by themselves. Leave the desktop alone: the screenshot is of the
# active window, and a shot is skipped if Konsole is not the active one.
# Turn on Do Not Disturb first: each window can post Konsole notifications.
# Look through the PNGs before you send them on.
#
# Environment: BUILD (default ./build), KONSOLE_SCHEMES (Konsole colour
# schemes, default: those bundled with Konsole that are installed), ONLY (a
# shot name prefix, to re-run a few).
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

# Under /tmp (not $TMPDIR, which may be in your home) and with a copy of the
# binary, so the windows show no home or checkout path.
work=$(mktemp -d /tmp/kyu-screens.XXXXXX)
pid=
holder=
cleanup() {
    [ -n "$pid" ] && kill "$pid" 2>/dev/null
    [ -n "$holder" ] && kill "$holder" 2>/dev/null
    rm -rf "$work"
}
trap cleanup EXIT
trap 'exit 130' INT TERM
mkdir -p "$out" "$work/bin"
cp "$build/bin/kyu-update" "$work/bin/"
: >"$out/screens.log"
log() { echo "$*" | tee -a "$out/screens.log"; }
failed=0
checked_fakes=

# The text Konsole shows, where this Konsole can say it (newer versions).
screen_text() {
    busctl --user call "$service" /Sessions/1 org.kde.konsole.Session getAllDisplayedText b true 2>/dev/null
}
# Konsole is the active window (so the screenshot is of it): "yes", "no",
# or "unknown" where the window does not say.
active() {
    local r
    r=$(busctl --user get-property "$service" /konsole/MainWindow_1 org.qtproject.Qt.QWidget isActiveWindow 2>/dev/null) || { echo unknown; return; }
    [ "$r" = "b true" ] && echo yes || echo no
}
send() {
    busctl --user call "$service" /Sessions/1 org.kde.konsole.Session sendText s "$1" >/dev/null 2>&1 || {
        log "FAILED $name: could not type into the window"
        failed=$((failed + 1))
    }
}

# shot NAME SCHEME SIZE MODE STATE STEPS...
#   SCHEME: "-" for your own profile's, else a Konsole colour scheme name.
#   SIZE: normal | narrow | wide (columns x rows).  MODE: KYU_FAKE_MODE.
#   STATE: "packages flatpaks news(0/1)", or "-" for 2 1 0.
#   Variables for one call: FLATPAK_EXIT=N (the fake flatpak fails),
#   RUNNING=VERSION (the running kernel; the default is older than the
#   installed one, so a restart is needed).
#   Setup steps (before the window opens): "lock" (a stale pacman lock),
#   "busy" (pacman holds it), "running" (another kyu-update holds Kyu's
#   lock), "oldlibs" (one of your programs uses a replaced library).
#   Steps: "until TEXT" (wait for TEXT on screen, where Konsole can say;
#   else 2 seconds), "wait SECONDS", "type TEXT" (then Enter), "enter",
#   "shot".
shot() {
    name=$1
    local scheme=$2 size=$3 mode=$4 st=$5
    shift 5
    case "$name" in "${ONLY:-}"*) ;; *) return ;; esac
    local d="$work/$name" step arg i
    rm -rf "$d"
    mkdir -p "$d"
    [ "$st" = - ] || fake_state "$d/state.json" $st
    fake_system "$d"
    local -a profile=(-p "LocalTabTitleFormat=Kyu test" -p "RemoteTabTitleFormat=Kyu test" -p TerminalRows=40)
    case "$size" in
    narrow) profile+=(-p TerminalColumns=60) ;;
    wide) profile+=(-p TerminalColumns=160) ;;
    *) profile+=(-p TerminalColumns=100) ;;
    esac
    [ "$scheme" = - ] || profile+=(-p "ColorScheme=$scheme")
    for step in "$@"; do
        case "$step" in
        lock) touch "$d/db.lck" ;;
        busy) touch "$d/db.lck"; mkdir -p "$d/proc/4242"; echo pacman >"$d/proc/4242/comm" ;;
        oldlibs)
            mkdir -p "$d/proc/5150"
            echo firefox >"$d/proc/5150/comm"
            echo "7f00-7f01 r--p 0 00:1f 9  /usr/lib/libmozgtk.so (deleted)" >"$d/proc/5150/maps"
            ;;
        running)
            # A live process holds Kyu's lock, in QLockFile's format.
            sleep 600 &
            holder=$!
            mkdir -p "$d/state/kyu"
            printf '%s\nsleep\n%s\n' "$holder" "$(cat /proc/sys/kernel/hostname)" >"$d/state/kyu/update.lock"
            ;;
        esac
    done
    # Konsole gets the fake state directory too: no remembered window size
    # overrides the columns, and your Konsole's saved state is not touched.
    # shellcheck disable=SC2046 # fake_args is a word list without spaces
    env PATH="$fakes:$PATH" KYU_FAKE_DIR="$d" KYU_FAKE_MODE="$mode" KYU_FAKE_FLATPAK_EXIT="${FLATPAK_EXIT:-0}" \
        XDG_STATE_HOME="$d/state" \
        konsole --separate --hide-menubar --hide-tabbar --workdir "$d" "${profile[@]}" \
        -e "$work/bin/kyu-update" $(fake_args "$d") >>"$work/konsole.log" 2>&1 &
    pid=$!
    service=org.kde.konsole-$pid
    for i in $(seq 75); do
        busctl --user introspect "$service" /Sessions/1 >/dev/null 2>&1 && break
        sleep 0.2
    done
    if ! busctl --user introspect "$service" /Sessions/1 >/dev/null 2>&1; then
        log "FAILED $name: Konsole did not appear on D-Bus"
        failed=$((failed + 1))
        close
        return
    fi
    local can_read=
    screen_text >/dev/null && can_read=1
    local n=0
    for step in "$@"; do
        arg=${step#* }
        case "$step" in
        lock | busy | oldlibs | running) ;;
        until\ *)
            if [ -n "$can_read" ]; then
                for i in $(seq 50); do
                    screen_text | grep -qF -- "$arg" && break
                    sleep 0.2
                done
                screen_text | grep -qF -- "$arg" || log "note $name: '$arg' did not appear"
                sleep 0.3
            else
                sleep 2
            fi
            ;;
        wait\ *) sleep "$arg" ;;
        type\ *) send "$arg"$'\r'; sleep 0.8 ;;
        enter) send $'\r'; sleep 0.8 ;;
        shot)
            n=$((n + 1))
            local label=$name
            [ $n -gt 1 ] && label="$name-$n"
            local file="$out/$label.png" state
            rm -f "$file"
            state=$(active)
            if [ "$state" = no ]; then
                # Something else has the focus: no screenshot of it, and stop
                # here rather than go on taking pictures of the wrong window.
                log "FAILED $label: Konsole is not the active window; stopping (leave the desktop alone while this runs)"
                failed=$((failed + 1))
                close
                exit 1
            fi
            spectacle --background --nonotify --activewindow --output "$file" >>"$work/konsole.log" 2>&1
            for i in $(seq 25); do [ -s "$file" ] && break; sleep 0.2; done
            if [ -s "$file" ]; then
                [ "$state" = yes ] && log "ok $label" || log "ok $label (could not confirm it is Konsole: check this one)"
            else
                log "FAILED $label: no screenshot"
                failed=$((failed + 1))
            fi
            [ -n "$can_read" ] && screen_text >"$out/$label.txt"
            ;;
        *)
            log "FAILED $name: unknown step '$step'"
            failed=$((failed + 1))
            ;;
        esac
    done
    # The first window that should run pacman proves the fakes are in use.
    [ -e "$d/pacman-ran" ] && checked_fakes=1
    close
    # What kyu-update left for the card (without this machine's boot id).
    [ -f "$d/state/kyu/last-run.json" ] && grep -v '"bootId"' "$d/state/kyu/last-run.json" >"$out/$name.last-run.json"
    if [ -n "$holder" ]; then kill "$holder" 2>/dev/null; holder=; fi
}

# Close as a person would: Enter at "Press Enter to close" ends kyu-update
# and Konsole closes with it (a second Enter if the first answered a
# question), else end the window.
close() {
    local try i
    for try in 1 2; do
        kill -0 "$pid" 2>/dev/null || break
        busctl --user call "$service" /Sessions/1 org.kde.konsole.Session sendText s $'\r' >/dev/null 2>&1
        for i in $(seq 15); do kill -0 "$pid" 2>/dev/null || break; sleep 0.2; done
    done
    if kill -0 "$pid" 2>/dev/null; then
        log "note $name: the window did not close after Enter"
        kill "$pid" 2>/dev/null
    fi
    wait "$pid" 2>/dev/null
    pid=
}

NEW=6.18.22-1-MANJARO # the installed kernel in the fake system

log "== your Konsole profile"
shot A-01-done - normal normal - "until Proceed with" "type y" "until Press Enter" shot
if [ -z "${ONLY:-}" ] && [ -z "$checked_fakes" ]; then
    log "FAILED: the fake pacman did not run in the first window; stopping"
    exit 1
fi
RUNNING=$NEW shot A-02-done-no-restart - normal normal - "until Proceed with" "type y" "until Press Enter" shot
RUNNING=$NEW shot A-03-systemd - normal systemd - "until Proceed with" "type y" "until Press Enter" shot
RUNNING=$NEW shot A-04-old-libraries - normal normal - oldlibs "until Proceed with" "type y" "until Press Enter" shot
shot A-05-news-question - normal normal "2 1 1" "until Continue?" shot "type y" "until Proceed with" "type y" "until Press Enter" shot
shot A-06-news-stopped - normal normal "2 1 1" "until Continue?" enter "until Press Enter" shot
shot A-07-downloading - normal slow - "until Proceed with" "type y" "wait 1" shot "until Press Enter" shot
shot A-08-keyring - normal keyring - "until Proceed with" "type y" "until Press Enter" shot
shot A-09-declined - normal normal - "until Proceed with" "type n" "until Press Enter" shot
shot A-10-failed-midway - normal midfail - "until Proceed with" "type y" "until Press Enter" shot
shot A-11-other-language - normal german - "until Press Enter" shot
shot A-12-stale-lock - normal normal - lock "until Press Enter" shot
shot A-13-busy - normal normal - busy "until Press Enter" shot
shot A-14-already-running - normal normal - running "until Press Enter" shot
shot A-15-flatpak-only - normal normal "0 2 0" "until Press Enter" shot
FLATPAK_EXIT=1 shot A-16-flatpak-failed - normal normal - "until Proceed with" "type y" "until Press Enter" shot
RUNNING=$NEW shot A-17-nothing-new - normal nothing "0 0 0" "until Press Enter" shot

log "== Konsole's colour schemes"
schemes=${KONSOLE_SCHEMES:-Breeze BlackOnWhite BlackOnLightYellow Solarized SolarizedLight DarkPastels WhiteOnBlack Linux GreenOnBlack}
for s in $schemes; do
    if [ ! -f "/usr/share/konsole/$s.colorscheme" ] && [ ! -f "$HOME/.local/share/konsole/$s.colorscheme" ]; then
        log "skip $s: no such Konsole colour scheme"
        continue
    fi
    shot "B-$s-done" "$s" normal normal - "until Proceed with" "type y" "until Press Enter" shot
    shot "B-$s-keyring" "$s" normal keyring - "until Proceed with" "type y" "until Press Enter" shot
    shot "B-$s-news" "$s" normal normal "2 1 1" "until Continue?" shot
done

log "== narrow (60 columns) and wide (160 columns)"
for z in narrow wide; do
    shot "C-$z-done" - "$z" normal - "until Proceed with" "type y" "until Press Enter" shot
    shot "C-$z-keyring" - "$z" keyring - "until Proceed with" "type y" "until Press Enter" shot
    shot "C-$z-news" - "$z" normal "2 1 1" "until Continue?" shot
done

cp "$work/konsole.log" "$out/konsole.log" 2>/dev/null
log "done: $(ls "$out"/*.png 2>/dev/null | wc -l) screenshots, $failed failed"
[ $failed -eq 0 ]
