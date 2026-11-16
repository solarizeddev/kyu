# Sourced by tests/kyu_update_test.sh and tools/kyu-update-screens.sh: a fake
# system for kyu-update in a directory, so nothing real is read or changed.

# fake_state FILE PACKAGES FLATPAKS NEWS(0/1): the checker's state file.
fake_state() {
    news=""
    [ "$4" = 1 ] && news='"items":[{"title":"Manual intervention for foo","link":"https://archlinux.org/news/foo/","published":"2099-01-01T00:00:00Z"}],'
    pk=""; i=0
    while [ $i -lt "$2" ]; do pk="$pk${pk:+,}{\"name\":\"p$i\",\"repo\":\"core\",\"oldVersion\":\"1\",\"newVersion\":\"2\",\"size\":1}"; i=$((i + 1)); done
    fp=""; i=0
    while [ $i -lt "$3" ]; do fp="$fp${fp:+,}{\"name\":\"org.x.App$i\",\"kind\":\"app\",\"branch\":\"stable\",\"origin\":\"flathub\",\"installation\":\"user\"}"; i=$((i + 1)); done
    cat >"$1" <<JSON
{"formatVersion":1,"checkedAt":"2026-11-16T10:00:00Z",
 "pacman":{"checked":true,"ok":true,"checkedAt":"2026-11-16T10:00:00Z"},
 "flatpak":{"checked":true,"ok":true,"checkedAt":"2026-11-16T10:00:00Z"},
 "packages":[$pk],"flatpaks":[$fp],
 "news":{"checked":true,"ok":true,$news"source":"Arch Linux news","url":"https://archlinux.org/feeds/news/","page":"https://archlinux.org/news/"}}
JSON
}

# fake_system DIR: state dir, pacman.log, /proc and modules dir in Manjaro's
# layout (the image named by kernelbase, not pkgbase). The running kernel is
# $RUNNING (default: an older one, so a restart is needed). Keeps an existing
# DIR/state.json (default: 2 packages, 1 Flatpak, no news).
fake_system() {
    mkdir -p "$1/state" "$1/proc/sys/kernel" "$1/modules/6.18.22-1-MANJARO" "$1/hooks"
    : >"$1/pacman.log"
    echo "${RUNNING:-6.18.20-1-MANJARO}" >"$1/proc/sys/kernel/osrelease"
    echo "BOOT_IMAGE=/boot/vmlinuz-6.18-x86_64 rw" >"$1/proc/cmdline"
    touch "$1/modules/6.18.22-1-MANJARO/vmlinuz"
    echo linux618 >"$1/modules/6.18.22-1-MANJARO/pkgbase"
    echo 6.18-x86_64 >"$1/modules/6.18.22-1-MANJARO/kernelbase"
    [ -f "$1/state.json" ] || fake_state "$1/state.json" 2 1 0
}

# fake_args DIR: kyu-update's options pointing at the fake system.
fake_args() {
    echo "--no-check --state-file $1/state.json --pacman-log $1/pacman.log --db-lock $1/db.lck --proc-dir $1/proc --modules-dir $1/modules --hooks-dir $1/hooks"
}
