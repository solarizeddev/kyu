# A mirror with today's package lists but not the packages: 404 on packages.
setup() {
    for r in core extra; do
        mkdir -p /srv/m/$r/os/x86_64
        curl -fsS -o /srv/m/$r/os/x86_64/$r.db "https://geo.mirror.pkgbuild.com/$r/os/x86_64/$r.db"
    done
    (cd /srv/m && setsid python -m http.server 8000 >/dev/null 2>&1 &)
    sleep 1
    printf 'Server = http://127.0.0.1:8000/$repo/os/$arch\n' > /etc/pacman.d/mirrorlist
}
