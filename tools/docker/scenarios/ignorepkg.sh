# less held back (IgnorePkg): not counted, not updated.
setup() { sed -i 's/^#IgnorePkg *=.*/IgnorePkg = less/' /etc/pacman.conf; grep ^IgnorePkg /etc/pacman.conf; }
