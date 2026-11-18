#!/bin/sh
# Manjaro fork: renames CachyOS's cachy-update to "Manjaro Update" in a copy
# of notifier/ (run by packaging/notifier/PKGBUILD in prepare()), so that
# notifier/ stays upstream's and merges cleanly:
# - the tray icon's click and "Open the Package Installer" (was "Run
#   Cachy-Update") start the Package Installer on its Updates page, through
#   the desktop file the notifier launches, instead of a terminal;
# - the icons are drawn from the Manjaro logo (make-icons.sh);
# - every other "Cachy-Update" and "CachyOS" becomes "Manjaro Update" and
#   "Manjaro". Fails when a CachyOS name is left over.
# Usage: rebrand.sh NOTIFIER_COPY
set -eu
here=$(cd "$(dirname "$0")" && pwd)
cd "$1"

# the action's text: in the code and as msgid; its old translations go
for f in src/tray/src/tray.rs src/lib/notification.sh; do
    sed -i 's/"Run Cachy-Update"/"Open the Package Installer"/g' "$f"
done
for f in po/*.po po/*.pot; do
    sed -i '/^msgid "Run Cachy-Update"$/{s//msgid "Open the Package Installer"/;n;s/^msgstr .*/msgstr ""/}' "$f"
done

grep -rl -e 'Cachy-Update' -e 'CachyOS' src res/desktop po | while read -r f; do
    sed -i -e 's/Cachy-Update/Manjaro Update/g' -e 's/CachyOS/Manjaro/g' "$f"
done

cp "$here/arch-update.desktop" res/desktop/arch-update.desktop
"$here/make-icons.sh" res/icons

# what is left may only be the icon file names the code asks for
left=$(grep -rn -i 'cachy' src res/desktop po | grep -v -e 'cachy-update-\$' -e 'cachy-update_updates-available-\$' \
    -e 'cachy-update-blue' -e 'cachy-update_updates-available-blue' || true)
if [ -n "$left" ]; then
    printf 'rebrand.sh: CachyOS names left:\n%s\n' "$left" >&2
    exit 1
fi
