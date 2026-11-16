#!/bin/sh
# One command for the first test on a real system. Builds Kyu, runs the
# automated tests, screenshots every screen of the panel across colour
# schemes, window sizes, fonts and scale factors (simulated system: nothing on
# your machine changes, no root), notes versions of the relevant packages,
# and packs everything into one zip to send back.
#
#   tools/kyu-test-all.sh                  writes ~/kyu-test.zip
#   tools/kyu-test-all.sh /path/name       writes /path/name.zip
#
# Takes about 10 minutes. Windows open and close by themselves while the
# screenshots run: leave the desktop alone. Your home directory is replaced
# by ~ in the logs. Environment: SCHEMES (see tools/kyu-screens.sh),
# BUILD (build directory, default ./build), KYU_CMAKE_ARGS (configure
# options, default -DKYU_REQUIRE_KSNI=ON).
set -u
src=$(cd "$(dirname "$0")/.." && pwd)
base=$(realpath -m "${1:-$HOME/kyu-test}")
out="$base"
build=$(realpath -m "${BUILD:-$src/build}")
rm -rf "$out"
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
        echo "colour scheme: $(kreadconfig6 --group General --key ColorScheme)"
        echo "font: $(kreadconfig6 --group General --key font)"
        echo "fixed font: $(kreadconfig6 --group General --key fixed)"
        echo "widget style: $(kreadconfig6 --group KDE --key widgetStyle)"
        echo "icons: $(kreadconfig6 --group Icons --key Theme)"
    fi
    command -v kscreen-doctor >/dev/null && { echo "screens:"; kscreen-doctor -o 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' | grep -E 'Output|Geometry|Scale|Modes' | sed 's/^/  /'; }
    echo "colour schemes installed: $(ls /usr/share/color-schemes "$HOME/.local/share/color-schemes" 2>/dev/null | sed -n 's/\.colors$//p' | sort -u | tr '\n' ' ')"
    echo "packages:"
    pacman -Q qt6-base qt6-declarative qt6-wayland kirigami qqc2-desktop-style kstatusnotifieritem kwindowsystem \
        plasma-workspace plasma-integration breeze flatpak pacman pacman-contrib polkit dbus rust cmake corrosion 2>&1 | sed 's/^/  /'
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

    step "Screenshots (screens/, about 8 minutes)"
    BUILD="$build" SCHEMES="${SCHEMES:-BreezeLight BreezeDark BreezeClassic}" \
        "$src/tools/kyu-screens.sh" "$out/screens" 2>&1 | tee "$out/screens.log" | grep -v ' ok$' || true
fi

# Your home directory and user name stay out of the zip's text files.
for f in "$out"/*.log "$out"/*.txt "$out"/screens/*.log; do
    [ -f "$f" ] && sed -i -e "s|$HOME|~|g" -e "s|/home/$(id -un)|~|g" "$f"
done
echo "result: $status" >>"$out/system.txt"

step "Zip"
rm -f "$base.zip"
if command -v bsdtar >/dev/null; then
    (cd "$(dirname "$out")" && bsdtar -a -cf "$base.zip" "$(basename "$out")")
else
    (cd "$(dirname "$out")" && zip -qr "$base.zip" "$(basename "$out")")
fi && echo "wrote $base.zip ($(du -h "$base.zip" | cut -f1)), result: $status"
