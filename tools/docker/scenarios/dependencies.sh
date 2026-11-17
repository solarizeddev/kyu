# An installed package needs the old zlib exactly: the update cannot go on.
setup() {
    v=$(pacman -Q zlib | cut -d' ' -f2)
    mkdir -p /tmp/dummy && cd /tmp/dummy
    cat > PKGBUILD <<PKG
pkgname=kyu-needs-old-zlib
pkgver=1
pkgrel=1
arch=(any)
depends=("zlib=$v")
package() { :; }
PKG
    chown -R tester /tmp/dummy
    su tester -c 'cd /tmp/dummy && makepkg -f >/dev/null'
    pacman -U --noconfirm /tmp/dummy/kyu-needs-old-zlib-1-1-any.pkg.tar.zst
}
