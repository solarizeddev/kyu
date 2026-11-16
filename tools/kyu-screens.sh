#!/bin/sh
# Screenshots of the panel across scenarios, colour schemes, window sizes,
# font sizes and scale factors, through tools/kyu-demo.sh (simulated system:
# nothing on your machine changes). Windows open and close by themselves;
# leave the desktop alone while it runs (about 5 minutes).
#
#   tools/kyu-screens.sh [OUTDIR]          default OUTDIR: ./kyu-screens
#
# Environment:
#   SCHEMES="BreezeLight BreezeDark"   colour schemes for section B; "all" for
#                                      every installed one. Your current scheme
#                                      is always in section A.
#   SECTIONS="A B C D E"               which sections to run
#   OFFSCREEN=1                        no windows (less faithful: no compositor)
#
# Sections:
#   A  every scenario, your theme, default size
#   B  colour schemes  x  list, question, finished, failure
#   C  window sizes    x  long list, question, finished
#   D  font sizes      x  long list, finished
#   E  scale factors   x  long list, finished
# If ImageMagick is installed, OUTDIR/sheet-*.png puts each section on one image.
set -u
src=$(cd "$(dirname "$0")/.." && pwd)
demo="$src/tools/kyu-demo.sh"
out=$(realpath -m "${1:-kyu-screens}")
mkdir -p "$out"
sections=${SECTIONS:-A B C D E}
failed=0

shot() { # NAME SCENARIO [VAR=value...]
    name=$1; scenario=$2; shift 2
    printf '%-44s' "$name"
    if env "$@" OUT="$out/$name.png" "$demo" "$scenario" >"$out/$name.log" 2>&1; then
        echo ok; rm -f "$out/$name.log"
    else
        echo "FAILED (see $name.log)"; failed=$((failed + 1))
    fi
}

sheet() { # PREFIX
    command -v montage >/dev/null || return 0
    prefix=$1
    set -- "$out/$prefix"-*.png
    [ -e "$1" ] || return 0
    montage -label '%t' -pointsize 14 "$@" -geometry '+8+8' -tile 6x -background '#888' "$out/sheet-$prefix.png" \
        || echo "(could not make sheet-$prefix.png)"
}

for s in $sections; do
    case "$s" in
    A)
        for scenario in $("$demo" --list | awk '{ print $1 }'); do
            shot "A-$scenario" "$scenario"
        done
        sheet A ;;
    B)
        schemes=${SCHEMES:-BreezeLight BreezeDark}
        if [ "$schemes" = all ]; then
            schemes=$(ls /usr/share/color-schemes "$HOME/.local/share/color-schemes" 2>/dev/null | sed -n 's/\.colors$//p' | sort -u)
        fi
        for scheme in $schemes; do
            for scenario in list q-provider finished fail-keyring; do
                shot "B-$scheme-$scenario" "$scenario" SCHEME="$scheme"
            done
        done
        sheet B ;;
    C)
        # Kyu's minimum is 18x25 grid units; the window manager keeps it there.
        for size in 300x420 400x600 1280x720 1920x1080 420x1000; do
            for scenario in list-long q-provider finished; do
                shot "C-$size-$scenario" "$scenario" SIZE="$size"
            done
        done
        sheet C ;;
    D)
        for pt in 8 12 16 20; do
            for scenario in list-long finished; do
                shot "D-${pt}pt-$scenario" "$scenario" FONT_PT="$pt"
            done
        done
        sheet D ;;
    E)
        for scale in 1.25 1.5 2; do
            for scenario in list-long finished; do
                shot "E-x$scale-$scenario" "$scenario" SCALE="$scale"
            done
        done
        sheet E ;;
    esac
done
echo "screenshots in $out ($failed failed)"
[ "$failed" -eq 0 ]
