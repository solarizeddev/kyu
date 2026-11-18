#!/bin/sh
# Every entry of pkglist.yaml in the repositories this system uses (run on
# Manjaro stable before a release): prints the entries that are not, and
# fails if there are any. Names are also checked against pacman's rules,
# since the app runs them in a shell command.
cd "$(dirname "$0")" || exit 2
bad=0
grep '^    - ' pkglist.yaml | sed 's/^    - //' | while read -r entry; do
    for p in $entry; do
        case "$p" in
        *[!a-z0-9@._+-]* | -*) echo "not a package name: $p"; exit 1 ;;
        esac
        pacman -Si "$p" >/dev/null 2>&1 || { echo "not in the repositories: $p"; exit 1; }
    done
done || bad=1
exit $bad
