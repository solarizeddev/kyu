#!/usr/bin/env python3
# Writes the test catalogs in tests/update/locale (gettext .mo, as msgfmt
# would): pacman.mo and libalpm.mo per language, with pacman's real
# translations of the messages Kyu recognises. Run it again after changing
# the entries below; the tests use the files it writes.
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))

# pacman's own translations (from its po files, GPL-2.0-or-later, as
# translated by pacman's translators), only the messages Kyu recognises.
REAL = {
    "de": {
        "error: ": "Fehler: ",
        "warning: ": "Warnung: ",
        "failed to commit transaction (%s)\n": "Der Vorgang konnte nicht durchgeführt werden (%s)\n",
        "failed to init transaction (%s)\n": "Der Vorgang konnte nicht gestartet werden (%s)\n",
        "could not lock database: %s\n": "Datenbank konnte nicht gesperrt werden: %s\n",
        "Errors occurred, no packages were upgraded.\n": "Fehler sind aufgetreten, keine Pakete wurden aktualisiert.\n",
        "%s: %s exists in filesystem\n": "%s: %s existiert im Dateisystem\n",
        "%s: %s exists in filesystem (owned by %s)\n": "%s: %s existiert im Dateisystem (gehört zu %s)\n",
        "failed retrieving file '%s' from %s : %s\n": "Konnte Datei '%s' nicht von %s übertragen : %s\n",
        "unable to lock database": "Kann Datenbank nicht sperren",
        "invalid or corrupted package (PGP signature)": "Ungültiges oder beschädigtes Paket (PGP-Signatur)",
        "conflicting files": "In Konflikt stehende Dateien",
        "could not satisfy dependencies": "Kann Abhängigkeiten nicht erfüllen",
        "Partition %s too full: %jd blocks needed, %ju blocks free\n": "Partition %s ist überfüllt: %jd Blöcke werden benötigt, %ju Blöcke sind frei\n",
        "not enough free disk space": "Nicht genug freier Festplattenspeicher",
        "%s exists in both '%s' and '%s'\n": "%s existiert in »%s« und »%s«\n",
    },
    "es": {
        "error: ": "error: ",
        "warning: ": "advertencia: ",
        "failed to commit transaction (%s)\n": "no se pudo realizar la operación (%s)\n",
        "failed to init transaction (%s)\n": "no se pudo iniciar la operación (%s)\n",
        "could not lock database: %s\n": "no se pudo bloquear la base de datos: %s\n",
        "%s: %s exists in filesystem (owned by %s)\n": "%s: %s ya existe en el sistema de archivos (es parte de %s)\n",
        "failed retrieving file '%s' from %s : %s\n": "no se pudo obtener el archivo «%s» desde %s: %s\n",
        "unable to lock database": "no se pudo bloquear la base de datos",
        "invalid or corrupted package (PGP signature)": "paquete no válido o dañado (firma PGP)",
        '%s: signature from "%s" is unknown trust\n': "%s: se desconoce el nivel de confianza de la firma de «%s»\n",
        "conflicting files": "archivos en conflicto",
    },
    "fr": {
        "error: ": "erreur\u00a0: ",
        "failed to commit transaction (%s)\n": "la validation de la transaction a échoué (%s)\n",
        "%s: %s exists in filesystem\n": "%s\u00a0: %s est déjà présent dans le système de fichiers\n",
        "conflicting files": "conflit de fichiers\u00a0",
        "failed retrieving file '%s' from %s : %s\n": "échec de récupération du fichier «\u00a0%s\u00a0» depuis %s\u00a0: %s\n",
    },
    "tr": {
        "error: ": "hata: ",
        "failed retrieving file '%s' from %s : %s\n": "%3$s hatası nedeniyle '%1$s' dosyası %2$s adresinden alınamadı\n",
        "Partition %s too full: %jd blocks needed, %ju blocks free\n": "%s diski çok dolu: %jd blok gerekiyor, %ju blok boşta\n\n",
        "failed to commit transaction (%s)\n": "işlem gerçekleştirilemedi (%s)\n",
        "not enough free disk space": "yetersiz disk alanı",
    },
    "zh_CN": {
        "error: ": "错误：",
        "failed to commit transaction (%s)\n": "无法提交处理 (%s)\n",
        "invalid or corrupted package (PGP signature)": "无效或已损坏的软件包 (PGP 签名)",
        '%s: signature from "%s" is unknown trust\n': '%s: 来自 "%s" 的签名是未知信任的\n',
        "failed retrieving file '%s' from %s : %s\n": "无法从 %2$s : %3$s 获取文件 '%1$s'\n",
    },
}
DOMAIN_OF = {"error: ": "pacman", "warning: ": "pacman", "Errors occurred, no packages were upgraded.\n": "pacman"}


def domain(msgid):
    if msgid in DOMAIN_OF or msgid.startswith(("failed to commit", "failed to init", "could not lock", "%s: %s exists", "%s exists in both")):
        return "pacman"
    return "libalpm"



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


for language, entries in REAL.items():
    os.makedirs(os.path.join(HERE, "locale", language, "LC_MESSAGES"), exist_ok=True)
    for name in ("pacman", "libalpm"):
        with open(os.path.join(HERE, "locale", language, "LC_MESSAGES", name + ".mo"), "wb") as f:
            f.write(mo({k: v for k, v in entries.items() if domain(k) == name}))
