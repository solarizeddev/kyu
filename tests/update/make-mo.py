#!/usr/bin/env python3
# Writes the German test catalogs in tests/update/locale (gettext .mo, as
# msgfmt would), shaped like pacman's own: pacman.mo and libalpm.mo. Run it
# again after changing the entries below; the tests use the files it writes.
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))

CATALOGS = {
    "pacman": {
        "error: ": "Fehler: ",
        "warning: ": "Warnung: ",
        "failed to commit transaction (%s)\n": "Konnte den Vorgang nicht durchführen (%s)\n",
        "failed to init transaction (%s)\n": "Konnte den Vorgang nicht starten (%s)\n",
        "could not lock database: %s\n": "Konnte die Datenbank nicht sperren: %s\n",
        "Errors occurred, no packages were upgraded.\n": "Fehler aufgetreten, keine Pakete wurden aktualisiert.\n",
        "%s: %s exists in filesystem\n": "%s: %s existiert im Dateisystem\n",
        ":: Proceed with installation? [Y/n] ": ":: Installation fortsetzen? [J/n] ",
    },
    "libalpm": {
        "failed retrieving file '%s' from %s : %s\n": "Konnte Datei '%s' nicht von %s übertragen : %s\n",
        "unable to lock database": "Konnte die Datenbank nicht sperren",
        "invalid or corrupted package (PGP signature)": "Ungültiges oder beschädigtes Paket (PGP-Signatur)",
        '%s: signature from "%s" is unknown trust\n': '%s: Signatur von "%s" hat unbekanntes Vertrauen\n',
        "conflicting files": "In Konflikt stehende Dateien",
        "could not satisfy dependencies": "Konnte Abhängigkeiten nicht erfüllen",
        # The arguments in another order, as translators may write them.
        "Partition %s too full: %jd blocks needed, %ju blocks free\n": "Partition %1$s zu voll: %3$ju Blöcke frei, %2$jd Blöcke benötigt\n",
        "not enough free disk space": "Nicht genügend freier Speicherplatz",
    },
}


def mo(entries):
    items = sorted(entries.items())
    ids = [k.encode() for k, _ in items]
    strs = [v.encode() for _, v in items]
    n = len(items)
    start = 28 + n * 16
    data = b""
    oid, ostr = [], []
    for s in ids:
        oid.append((len(s), start + len(data)))
        data += s + b"\0"
    for s in strs:
        ostr.append((len(s), start + len(data)))
        data += s + b"\0"
    head = struct.pack("<7I", 0x950412DE, 0, n, 28, 28 + n * 8, 0, 0)
    table = b"".join(struct.pack("<2I", *e) for e in oid) + b"".join(struct.pack("<2I", *e) for e in ostr)
    return head + table + data


for domain, entries in CATALOGS.items():
    with open(os.path.join(HERE, "locale", "de", "LC_MESSAGES", domain + ".mo"), "wb") as f:
        f.write(mo(entries))
