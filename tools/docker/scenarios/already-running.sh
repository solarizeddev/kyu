# A second kyu-update while one waits at its password: "already running".
setup() { :; }
drive() {
    ( DRIVE_PASSWORD_CTRLC=0 DRIVE_KEYS= python /drive.py /out/first -- bash -c 'sleep 1; kyu-update' ) &
    sleep 8
    kyu_run second
    wait
}
