#!/bin/sh
# One command for the first test on a real system. Builds Kyu, runs the
# automated tests, screenshots every screen of the panel across colour
# schemes, window sizes, fonts and scale factors (simulated system: nothing on
# your machine changes, no root), notes versions of the relevant packages,
# and packs everything into one zip to send back.
#
#   tools/kyu-test-all.sh                  writes ~/kyu-test.zip
#   tools/kyu-test-all.sh /path/name.zip   writes that zip
#
# Takes about 10 minutes. Windows open and close by themselves while the
# screenshots run: leave the desktop alone. Your home directory, user name
# and the checkout's path are replaced in the logs. Environment: SCHEMES (see
# tools/kyu-screens.sh), BUILD (build directory, default ./build),
# KYU_CMAKE_ARGS (configure options, default -DKYU_REQUIRE_KSNI=ON).
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
    echo "packages:"
    pacman -Q qt6-base qt6-declarative qt6-wayland kirigami qqc2-desktop-style kstatusnotifieritem kwindowsystem kconfig \
        plasma-workspace plasma-integration libkscreen breeze flatpak pacman pacman-contrib polkit dbus \
        rust cmake ninja corrosion imagemagick 2>&1 | sed 's/^/  /'
} >"$out/system.txt" 2>&1
cat "$out/system.txt"

status=ok
step "Build (build.log)"
# A build directory that already exists keeps its generator.
generator=
[ -f "$build/CMakeCache.txt" ] || generator="-G Ninja"
# shellcheck disable=SC2086 # word lists
if cmake -S "$src" -B "$build" $generator ${KYU_CMAKE_ARGS--DKYU_REQUIRE_KSNI=ON} >"$out/build.log" 2>&1 &&
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

    step "Screenshots (screens/, about 8 minutes; one line per screenshot)"
    BUILD="$build" SCHEMES="${SCHEMES:-BreezeLight BreezeDark BreezeClassic}" \
        "$src/tools/kyu-screens.sh" "$out/screens" 2>&1 | tee "$out/screens.log"
    if grep -q FAILED "$out/screens.log"; then
        [ "$status" = ok ] && status="screenshots failed" || status="$status, screenshots failed"
    fi
fi
echo "result: $status" >>"$out/system.txt"

# Your home directory, user name and the checkout's path stay out of the
# zip's text files. (Literal replacements: no regex characters involved.)
user=$(id -un)
for f in "$out"/*.log "$out"/*.txt "$out"/screens/*.log; do
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
else
    # Keep the results rather than lose ten minutes of work.
    trap - EXIT
    echo "kyu-test-all: could not write $zipfile; the results are in $out" >&2
    exit 1
fi
