#!/bin/sh
# Screenshots of the panel across scenarios, colour schemes, window sizes,
# font sizes and scale factors, through tools/kyu-demo.sh (simulated system:
# nothing on your machine changes). Windows open and close by themselves;
# leave the desktop alone while it runs (about 15 minutes for both looks).
#
#   tools/kyu-screens.sh [OUTDIR]          default OUTDIR: ./kyu-screens
#
# Environment:
#   SCHEMES="BreezeLight BreezeDark"   colour schemes for section B; "all" for
#                                      every installed one. Your current scheme
#                                      is always in section A.
#   SECTIONS="A B C D E"               which sections to run
#   LOOKS="plasma terminal"            which looks of the panel
#   OFFSCREEN=1                        no windows (less faithful: no compositor)
#
# Sections (each for every look; names start with the section and the look):
#   A  every scenario, your theme, default size
#   B  colour schemes  x  list, question, finished, failure
#   C  window sizes    x  long list, question, finished
#   D  font sizes      x  long list, finished
#   E  scale factors   x  long list, finished
# If ImageMagick is installed, OUTDIR/sheet-<section>-<look>.png puts each on one image.
set -u
src=$(cd "$(dirname "$0")/.." && pwd)
demo="$src/tools/kyu-demo.sh"
out=$(realpath -m "${1:-kyu-screens}")
mkdir -p "$out"
sections=${SECTIONS:-A B C D E}
looks=${LOOKS:-plasma terminal}
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
  for look in $looks; do
    p="$s-$look"
    case "$s" in
    A)
        for scenario in $("$demo" --list | awk '{ print $1 }'); do
            shot "$p-$scenario" "$scenario" LOOK="$look"
        done ;;
    B)
        schemes=${SCHEMES:-BreezeLight BreezeDark}
        if [ "$schemes" = all ]; then
            schemes=$(ls /usr/share/color-schemes "$HOME/.local/share/color-schemes" 2>/dev/null | sed -n 's/\.colors$//p' | sort -u)
        fi
        for scheme in $schemes; do
            for scenario in list q-provider finished fail-keyring; do
                shot "$p-$scheme-$scenario" "$scenario" LOOK="$look" SCHEME="$scheme"
            done
        done ;;
    C)
        # Kyu's minimum is 18x25 grid units; the window manager keeps it there.
        for size in 300x420 400x600 1280x720 1920x1080 420x1000; do
            for scenario in list-long q-provider finished; do
                shot "$p-$size-$scenario" "$scenario" LOOK="$look" SIZE="$size"
            done
        done ;;
    D)
        for pt in 8 12 16 20; do
            for scenario in list-long finished; do
                shot "$p-${pt}pt-$scenario" "$scenario" LOOK="$look" FONT_PT="$pt"
            done
        done ;;
    E)
        for scale in 1.25 1.5 2; do
            for scenario in list-long finished; do
                shot "$p-x$scale-$scenario" "$scenario" LOOK="$look" SCALE="$scale"
            done
        done ;;
    esac
    sheet "$p"
  done
done
echo "screenshots in $out ($failed failed)"
[ "$failed" -eq 0 ]
