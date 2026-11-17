# docker: --tmpfs /home/tester/.local/state:size=24k,uid=1000,gid=1000
# Kyu's state directory nearly full: the recording cannot be written.
setup() { id tester; df -h /home/tester/.local/state; }
drive() {
    mkdir -p $state; dd if=/dev/zero of=$state/../filler bs=1k count=20 2>/dev/null
    kyu_run update
}
