# An AUR package whose build fails (its sources are on a blocked host); then
# a second check and run: is it offered again?
setup() {
    mkdir -p /tmp/gc && cd /tmp/gc
    cat > PKGBUILD <<'PKG'
pkgname=google-chrome
pkgver=1.0
pkgrel=1
arch=(any)
package() { :; }
PKG
    chown -R tester /tmp/gc
    su tester -c 'cd /tmp/gc && makepkg -f >/dev/null'
    pacman -U --noconfirm /tmp/gc/google-chrome-1.0-1-any.pkg.tar.zst
}
drive() {
    kyu-check --json --aur-now > /out/check-1.json 2>/out/check-1.err
    kyu_run first
    kyu_check check-2
    kyu_run second
    QT_QPA_PLATFORM=offscreen kyu --screenshot /out/card.png 2>/dev/null
}
