# German: no connection, untrusted keys, and Flatpak declined and gone.
setup() {
    set -e
    sed -i 's/^NoExtract/#NoExtract/' /etc/pacman.conf
    pacman -Sy --noconfirm glibc pacman sudo flatpak
    sed -i 's/^#de_DE.UTF-8/de_DE.UTF-8/' /etc/locale.gen
    locale-gen
    mkdir -p /srv/fpsrc/usr/share /srv/fpsrc/files/share
    printf '[Runtime]\nname=org.kyu.Platform\nruntime=org.kyu.Platform/x86_64/1\nsdk=org.kyu.Platform/x86_64/1\n' > /srv/fpsrc/metadata
    echo one | tee /srv/fpsrc/usr/share/kyu.txt > /srv/fpsrc/files/share/kyu.txt
    flatpak build-export --runtime /srv/fp /srv/fpsrc 1; chmod -R a+rwX /srv
    su tester -c 'flatpak remote-add --user --no-gpg-verify kyu file:///srv/fp && flatpak install --user -y --noninteractive kyu org.kyu.Platform//1'
    echo two | tee /srv/fpsrc/usr/share/kyu.txt > /srv/fpsrc/files/share/kyu.txt
    flatpak build-export --runtime /srv/fp /srv/fpsrc 1; chmod -R a+rwX /srv
    cp /etc/pacman.d/mirrorlist /etc/pacman.d/mirrorlist.good
    printf 'Server = http://127.0.0.1:9/$repo/os/$arch\n' > /etc/pacman.d/mirrorlist
}
drive() {
    export LANG=de_DE.UTF-8 LC_ALL=de_DE.UTF-8
    kyu_run nonet
}
after() {
    cp /etc/pacman.d/mirrorlist.good /etc/pacman.d/mirrorlist
    pacman -Syu --noconfirm >/dev/null 2>&1
    su tester -c 'export HOME=/home/tester LANG=de_DE.UTF-8 LC_ALL=de_DE.UTF-8; cd; source /lib.sh; kyu_check fp-before; DRIVE_FLATPAK=n kyu_run fp-declined; mv /srv/fp /srv/fp-gone; kyu_run fp-gone' >> /out/drive.log 2>&1
    mv /srv/fp-gone /srv/fp
    rm -rf /etc/pacman.d/gnupg; pacman-key --init >/dev/null 2>&1; pacman-key --add /usr/share/pacman/keyrings/archlinux.gpg >/dev/null 2>&1
    # Something to update, signed by keys no longer trusted.
    pacman -Syy >/dev/null 2>&1
    v=$(pacman -Si nano | sed -n 's/^Version *: //p')
    d=/var/lib/pacman/local/nano-$(pacman -Q nano | cut -d' ' -f2)
    sed -i "/^%VERSION%/{n;s/.*/0.1-1/}" "$d/desc"
    su tester -c 'export HOME=/home/tester LANG=de_DE.UTF-8 LC_ALL=de_DE.UTF-8; cd; source /lib.sh; kyu_run keyring' >> /out/drive.log 2>&1
}
