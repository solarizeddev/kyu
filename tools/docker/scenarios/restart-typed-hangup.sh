# "y" typed at "Restart now?" (no Enter), then the window closed: no reboot
# is attempted (big set: systemd updated, so [r] is offered).
setup() { :; }
export DRIVE_KEYS="r," DRIVE_TYPE_HANGUP="Restart now|y"
after() { : ; }
