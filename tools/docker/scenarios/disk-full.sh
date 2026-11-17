# docker: --tmpfs /var/cache/pacman/pkg:size=200k
# No room for the downloads.
setup() { df -h /var/cache/pacman/pkg; }
