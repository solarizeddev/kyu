#!/bin/sh
# Every entry of pkglist.yaml in the repositories this system uses (run on
# Manjaro stable after merging an upstream release): prints every entry
# that is not, and fails if there is any. Names are also checked against
# pacman's rules, since the program runs them in a shell command.
cd "$(dirname "$0")" || exit 2
bad=0
for p in $(grep '^    - ' pkglist.yaml | sed 's/^    - //'); do
    case "$p" in
    *[!a-z0-9@._+-]* | -*) echo "not a package name: $p"; bad=1; continue ;;
    esac
    pacman -Si "$p" >/dev/null 2>&1 || { echo "not in the repositories: $p"; bad=1; }
done
exit $bad
