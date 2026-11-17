# nano held back (IgnorePkg): not counted, not updated (1 package: zlib).
setup() { sed -i 's/^#IgnorePkg *=.*/IgnorePkg = nano/' /etc/pacman.conf; grep ^IgnorePkg /etc/pacman.conf; }
