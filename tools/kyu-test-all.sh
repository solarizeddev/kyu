#!/bin/sh
# One command for a test on a real system. Builds Kyu, runs the automated
# tests, screenshots the card in each state (as a window, with your window
# decoration) and kyu-update in Kyu's own window for every outcome,
# in your profile, Konsole's colour schemes and window sizes (a fake system:
# fake sudo, pacman and flatpak; nothing on your machine changes, no root),
# keeps the text of each run, notes versions and the facts kyu-update reads
# (kernel module directory, boot image, pamac), and packs it into one zip.
#
#   tools/kyu-test-all.sh                  writes ~/kyu-test.zip
#   tools/kyu-test-all.sh /path/name.zip   writes that zip
#
# Takes about 6 minutes. Turn on Do Not Disturb first, then leave the
# desktop alone: Kyu's windows open and close by themselves and each
# screenshot is of the active window. Look through screens/ before you send
# the zip. Your home directory, user name and the checkout's path are
# replaced in the text files. Environment: BUILD (build directory, default
# ./build), KONSOLE_SCHEMES and UNCHECKED (see tools/kyu-update-screens.sh),
# KYU_CMAKE_ARGS (configure options, default -DKYU_REQUIRE_KSNI=ON
# -DKYU_REQUIRE_WINDOW=ON).
set -u
src=$(cd "$(dirname "$0")/.." && pwd)
zipfile=$(realpath -m "${1:-$HOME/kyu-test.zip}")
case "$zipfile" in *.zip) ;; *) zipfile="$zipfile.zip" ;; esac
[ -d "$zipfile" ] && { echo "kyu-test-all: $zipfile is a directory" >&2; exit 2; }
build=$(realpath -m "${BUILD:-$src/build}")
# Everything is collected in a new temporary directory; only the zip is
# written where you asked.
stage=$(mktemp -d "${TMPDIR:-/tmp}/kyu-test.XXXXXX")
trap 'rm -rf "$stage"' EXIT
trap 'exit 130' INT TERM
out="$stage/kyu-test"
mkdir -p "$out"
step() { printf '\n== %s\n' "$1"; }

step "System"
{
    echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "kyu: $(git -C "$src" describe --always --dirty 2>/dev/null) on $(git -C "$src" rev-parse --abbrev-ref HEAD 2>/dev/null)"
    echo "kernel: $(uname -r) $(uname -m)"
    grep -E '^(NAME|ID|ID_LIKE|VERSION_ID|BUILD_ID)=' /etc/os-release
    echo "session: XDG_SESSION_TYPE=${XDG_SESSION_TYPE:-} XDG_CURRENT_DESKTOP=${XDG_CURRENT_DESKTOP:-} QT_QPA_PLATFORMTHEME=${QT_QPA_PLATFORMTHEME:-} QT_SCALE_FACTOR=${QT_SCALE_FACTOR:-}"
    if command -v kreadconfig6 >/dev/null; then
        # Empty means the default (Breeze Light, Noto Sans 10, Breeze).
        echo "colour scheme: $(kreadconfig6 --group General --key ColorScheme --default '(default)')"
        echo "font: $(kreadconfig6 --group General --key font --default '(default)')"
        echo "fixed font: $(kreadconfig6 --group General --key fixed --default '(default)')"
        echo "widget style: $(kreadconfig6 --group KDE --key widgetStyle --default '(default)')"
        echo "icons: $(kreadconfig6 --group Icons --key Theme --default '(default)')"
    fi
    command -v kscreen-doctor >/dev/null && { echo "screens:"; kscreen-doctor -o 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' | grep -E 'Output|Geometry|Scale|Modes' | sed 's/^/  /'; }
    echo "colour schemes installed: $(ls /usr/share/color-schemes "$HOME/.local/share/color-schemes" 2>/dev/null | sed -n 's/\.colors$//p' | sort -u | tr '\n' ' ')"
    echo "kyu-update reads:"
    echo "  running kernel: $(uname -r)"
    echo "  its module directory: $(ls /usr/lib/modules/"$(uname -r)"/ 2>&1 | grep -E '^(vmlinuz|pkgbase|kernelbase|modules.dep)$|No such' | tr '\n' ' ')"
    for m in /usr/lib/modules/*/; do
        [ -d "$m" ] || continue
        echo "  $(basename "$m"): pkgbase=$(cat "$m/pkgbase" 2>/dev/null) kernelbase=$(cat "$m/kernelbase" 2>/dev/null) vmlinuz=$([ -f "$m/vmlinuz" ] && echo yes || echo no)"
    done
    echo "  boot image: $(tr ' ' '\n' </proc/cmdline | grep '^BOOT_IMAGE=')"
    echo "  package managers running: $(pgrep -l 'pamac|packagekit|^pacman$' | tr '\n' ' ')"
    echo "  pacman.log readable: $([ -r /var/log/pacman.log ] && echo yes || echo no)"
    echo "  shell: $SHELL; sudo: $(command -v sudo || echo none); doas: $(command -v doas || echo none)"
    if command -v kreadconfig6 >/dev/null; then
        profile=$(kreadconfig6 --file konsolerc --group "Desktop Entry" --key DefaultProfile)
        echo "  konsole profile: ${profile:-(built-in)}, colour scheme: $( [ -n "$profile" ] && kreadconfig6 --file "$HOME/.local/share/konsole/$profile" --group Appearance --key ColorScheme)"
    fi
    echo "packages:"
    pacman -Q qt6-base qt6-wayland kstatusnotifieritem kwindowsystem kparts kcoreaddons kconfig \
        plasma-workspace plasma-integration libkscreen breeze flatpak pacman pacman-contrib polkit dbus \
        konsole spectacle util-linux sudo pamac-gtk pamac-cli \
        cmake ninja 2>&1 | sed 's/^/  /'
} >"$out/system.txt" 2>&1
cat "$out/system.txt"

status=ok
step "Build (build.log)"
# A build directory that already exists keeps its generator.
generator=
[ -f "$build/CMakeCache.txt" ] || generator="-G Ninja"
# shellcheck disable=SC2086 # word lists
if cmake -S "$src" -B "$build" $generator ${KYU_CMAKE_ARGS--DKYU_REQUIRE_KSNI=ON -DKYU_REQUIRE_WINDOW=ON} >"$out/build.log" 2>&1 &&
    cmake --build "$build" >>"$out/build.log" 2>&1; then
    tail -n 3 "$out/build.log"
else
    status="build failed"
    tail -n 30 "$out/build.log"
fi

if [ "$status" = ok ]; then
    step "Automated tests (ctest.log)"
    ctest --test-dir "$build" --output-on-failure >"$out/ctest.log" 2>&1 || status="tests failed"
    tail -n 4 "$out/ctest.log"

    step "kyu-update runs, as text (transcripts/)"
    mkdir -p "$out/transcripts" "$stage/tmp"
    TMPDIR="$stage/tmp" KEEP=1 sh "$src/tests/kyu_update_test.sh" "$build/bin" >"$out/transcripts/test.log" 2>&1 || status="kyu-update test failed"
    for f in "$stage"/tmp/*/*/out; do
        [ -f "$f" ] && cp "$f" "$out/transcripts/$(basename "$(dirname "$f")").txt"
    done
    tail -n 1 "$out/transcripts/test.log"

    step "The card's states (cards/)"
    mkdir -p "$out/cards"
    # Each card as a window, with your window decoration (Spectacle, the
    # active window); without Spectacle, the card's contents alone.
    for state in updates flatpaks uptodate checking checkfailed updating restart settings failed; do
        file="$out/cards/$state.png"
        if command -v spectacle >/dev/null; then
            rm -f "$stage/card-active"
            KYU_CARD_ACTIVE_FILE="$stage/card-active" "$build/bin/kyu" --demo "$state" >/dev/null 2>&1 &
            card=$!
            for i in $(seq 50); do [ "$(cat "$stage/card-active" 2>/dev/null)" = 1 ] && break; sleep 0.2; done
            if [ "$(cat "$stage/card-active" 2>/dev/null)" = 1 ]; then
                sleep 0.5 # the window's opening animation
                spectacle --background --nonotify --activewindow --output "$file" >/dev/null 2>&1
                for i in $(seq 25); do [ -s "$file" ] && break; sleep 0.2; done
            fi
            kill "$card" 2>/dev/null
            wait "$card" 2>/dev/null
        else
            timeout 20 "$build/bin/kyu" --demo "$state" --screenshot "$file" >/dev/null 2>&1
        fi
        if [ ! -s "$file" ]; then
            echo "card $state: failed (the card was not the active window, or no screenshot)"
            [ "$status" = ok ] && status="card screenshots failed" || status="$status, card screenshots failed"
        fi
    done
    ls "$out/cards" | wc -l | sed 's/$/ card screenshots/'

    step "kyu-update in Kyu's window (screens/, about 5 minutes; one line per screenshot)"
    BUILD="$build" "$src/tools/kyu-update-screens.sh" "$out/screens" 2>&1 | tee "$out/screens-run.log"
    if ! grep -q '^done:' "$out/screens/screens.log" 2>/dev/null || grep -q FAILED "$out/screens/screens.log"; then
        [ "$status" = ok ] && status="screenshots failed" || status="$status, screenshots failed"
    fi

fi
echo "result: $status" >>"$out/system.txt"

# Your home directory, user name and the checkout's path stay out of the
# zip's text files. (Literal replacements: no regex characters involved.)
user=$(id -un)
for f in "$out"/*.log "$out"/*.txt "$out"/screens/*.log "$out"/screens/*.json "$out"/screens/*.txt "$out"/transcripts/*; do
    [ -f "$f" ] || continue
    KYU_SRC="$src" KYU_HOME="$HOME" KYU_USER="$user" awk '
        function swap(s, from, to,   i, r) {
            if (from == "") return s
            r = ""
            while ((i = index(s, from)) > 0) { r = r substr(s, 1, i - 1) to; s = substr(s, i + length(from)) }
            return r s
        }
        # The user name only as a path component ("/name/"): as a bare word
        # it could be "test" and garble the logs.
        BEGIN { src = ENVIRON["KYU_SRC"]; home = ENVIRON["KYU_HOME"]; user = ENVIRON["KYU_USER"] }
        { $0 = swap($0, src, "<kyu>"); $0 = swap($0, home, "~"); print swap($0, "/" user "/", "/<user>/") }' "$f" >"$f.tmp" && mv "$f.tmp" "$f"
done

step "Zip"
rm -f "$zipfile"
mkdir -p "$(dirname "$zipfile")"
if command -v bsdtar >/dev/null; then
    (cd "$stage" && bsdtar -a -cf "$zipfile" kyu-test)
else
    (cd "$stage" && zip -qr "$zipfile" kyu-test)
fi
if [ $? -eq 0 ]; then
    echo "wrote $zipfile ($(du -h "$zipfile" | cut -f1)), result: $status"
    echo "Before you send it, unzip it and look through kyu-test/screens/: every picture should be a Kyu window (Konsole in the fallback); and kyu-test/cards/."
else
    # Keep the results rather than lose ten minutes of work.
    trap - EXIT
    echo "kyu-test-all: could not write $zipfile; the results are in $out" >&2
    exit 1
fi
