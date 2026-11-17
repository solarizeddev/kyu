# A mirror path that does not exist: 404 on the package lists themselves.
# (Fable: likely misread as "a mirror is out of date", with the lists warning.)
setup() { printf 'Server = https://geo.mirror.pkgbuild.com/nope/$repo/os/$arch\n' > /etc/pacman.d/mirrorlist; }
