# No terminal: kyu-update < /dev/null (no questions can be answered).
setup() { :; }
drive() { kyu-update < /dev/null > /out/no-terminal.txt 2>&1; echo "exit $?" >> /out/no-terminal.txt; }
