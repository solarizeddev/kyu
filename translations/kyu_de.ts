<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="de_DE">
<context>
    <name>Kyu</name>
    <message>
        <location filename="../src/client/failuretext.cpp" line="+46"/>
        <source>No connection to the mirrors</source>
        <translation>Keine Verbindung zu den Spiegelservern</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>pacman could not reach the mirrors. Check the network connection, then update again.</source>
        <translation>pacman konnte die Spiegelserver nicht erreichen. Die Netzwerkverbindung prüfen und dann erneut aktualisieren.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>pacman could not reach %1. Check the network connection, then update again.</source>
        <translation>pacman konnte %1 nicht erreichen. Die Netzwerkverbindung prüfen und dann erneut aktualisieren.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>The mirror does not have the package lists</source>
        <translation>Dem Spiegelserver fehlen die Paketlisten</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>The mirror answered &quot;404 Not Found&quot; for the package lists themselves: the mirror&apos;s address is wrong, or the mirror is gone.</source>
        <translation>Der Spiegelserver hat auf die Paketlisten selbst mit „404 Not Found“ geantwortet: Seine Adresse ist falsch, oder es gibt ihn nicht mehr.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 answered &quot;404 Not Found&quot; for the package lists themselves: the mirror&apos;s address is wrong, or the mirror is gone.</source>
        <translation>%1 hat auf die Paketlisten selbst mit „404 Not Found“ geantwortet: Die Adresse des Spiegelservers ist falsch, oder es gibt ihn nicht mehr.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>A new mirror list fixes it.</source>
        <translation>Eine neue Spiegelserverliste behebt das.</translation>
    </message>
    <message>
        <location line="+2"/>
        <location line="+18"/>
        <source>Mirrors are listed in %1.</source>
        <translation>Die Spiegelserver sind in %1 eingetragen.</translation>
    </message>
    <message>
        <location line="-14"/>
        <source>A mirror is out of date</source>
        <translation>Ein Spiegelserver ist veraltet</translation>
    </message>
    <message numerus="yes">
        <location line="+1"/>
        <source>The mirror had the new package lists but not %n package files (404 Not Found).</source>
        <comment>The mirror had the new package lists but not the package file below (404 Not Found).</comment>
        <translation>
            <numerusform>Der Spiegelserver hatte die neuen Paketlisten, aber nicht die unten genannte Paketdatei (404 Not Found).</numerusform>
            <numerusform>Der Spiegelserver hatte die neuen Paketlisten, aber nicht %n Paketdateien (404 Not Found).</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location line="+2"/>
        <source>%1 had the new package lists but not %n package files (404 Not Found).</source>
        <comment>%1 had the new package lists but not the package file below (404 Not Found).</comment>
        <translation>
            <numerusform>%1 hatte die neuen Paketlisten, aber nicht die unten genannte Paketdatei (404 Not Found).</numerusform>
            <numerusform>%1 hatte die neuen Paketlisten, aber nicht %n Paketdateien (404 Not Found).</numerusform>
        </translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Mirrors copy the main repositories with a delay. Try again later, or use other mirrors.</source>
        <translation>Spiegelserver übernehmen die Hauptpaketquellen mit Verzögerung. Später erneut versuchen oder andere Spiegelserver verwenden.</translation>
    </message>
    <message>
        <location line="+12"/>
        <location line="+73"/>
        <source>Package signatures could not be checked</source>
        <translation>Paketsignaturen konnten nicht geprüft werden</translation>
    </message>
    <message>
        <location line="-72"/>
        <source>pacman does not trust the key that signed some packages.</source>
        <translation>pacman vertraut dem Schlüssel nicht, mit dem einige Pakete signiert sind.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>pacman does not trust the key that signed %1.</source>
        <translation>pacman vertraut dem Schlüssel nicht, der %1 signiert hat.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Usually the keyring is older than the packages: updating the keyring first, then everything else, fixes it.</source>
        <translation>Meist ist der Schlüsselbund älter als die Pakete. Lösung: zuerst den Schlüsselbund, dann alles andere aktualisieren.</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>A downloaded package is damaged</source>
        <translation>Ein heruntergeladenes Paket ist beschädigt</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>A package in pacman&apos;s cache does not match its signature: the download was damaged or cut short, not the keyring.</source>
        <translation>Ein Paket im pacman-Cache passt nicht zu seiner Signatur: Der Download wurde beschädigt oder abgebrochen; am Schlüsselbund liegt es nicht.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 in pacman&apos;s cache does not match its signature: the download was damaged or cut short, not the keyring.</source>
        <translation>%1 im pacman-Cache passt nicht zu seiner Signatur: Der Download wurde beschädigt oder abgebrochen; am Schlüsselbund liegt es nicht.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>If pacman deleted it (its question above), updating again downloads it anew; otherwise remove it from %1 first.</source>
        <translation>Wenn pacman die Datei gelöscht hat (seine Frage oben), lädt eine erneute Aktualisierung sie neu herunter; andernfalls die Datei zuerst aus %1 entfernen.</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>Files already exist</source>
        <translation>Dateien sind bereits vorhanden</translation>
    </message>
    <message numerus="yes">
        <location line="+1"/>
        <source>%n files the update would install are already on disk, and no installed package owns them.</source>
        <comment>One file the update would install is already on disk, and no installed package owns it.</comment>
        <translation>
            <numerusform>Eine Datei, die die Aktualisierung installieren würde, ist bereits vorhanden und gehört keinem installierten Paket.</numerusform>
            <numerusform>%n Dateien, die die Aktualisierung installieren würde, sind bereits vorhanden und gehören keinem installierten Paket.</numerusform>
        </translation>
    </message>
    <message>
        <location line="+3"/>
        <source>pacman does not overwrite such files. Find out where they came from (%1 shows a file&apos;s package) before removing or moving them.</source>
        <translation>pacman überschreibt solche Dateien nicht. Vor dem Entfernen oder Verschieben herausfinden, woher sie stammen (%1 zeigt das Paket einer Datei).</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Not enough disk space</source>
        <translation>Nicht genügend Speicherplatz</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>pacman needs more free space than there is (in pacman&apos;s messages). Removing old package versions from pacman&apos;s cache often frees enough.</source>
        <translation>pacman benötigt mehr freien Speicherplatz als vorhanden (siehe die pacman-Meldungen). Das Entfernen alter Paketversionen aus dem pacman-Cache schafft oft genug Platz.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Dependencies cannot be satisfied</source>
        <translation>Abhängigkeiten können nicht erfüllt werden</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>pacman cannot resolve the update&apos;s dependencies (in pacman&apos;s messages). This often involves a package from outside the repos, such as the AUR, or a package held back with IgnorePkg.</source>
        <translation>pacman kann die Abhängigkeiten der Aktualisierung nicht auflösen (siehe die pacman-Meldungen). Oft ist ein Paket von außerhalb der Paketquellen beteiligt, etwa aus dem AUR, oder ein mit IgnorePkg zurückgehaltenes Paket.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>pacman&apos;s database is locked</source>
        <translation>Die pacman-Datenbank ist gesperrt</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Another package manager was using pacman&apos;s database.</source>
        <translation>Eine andere Paketverwaltung hat die pacman-Datenbank verwendet.</translation>
    </message>
    <message>
        <location filename="../src/core/aftercare.cpp" line="+191"/>
        <source>new kernel %1</source>
        <translation>neuer Kernel %1</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>the running kernel %1 was removed</source>
        <translation>der laufende Kernel %1 wurde entfernt</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>the running kernel was replaced</source>
        <translation>der laufende Kernel wurde ersetzt</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>%1 was updated</source>
        <translation>%1 wurde aktualisiert</translation>
    </message>
    <message>
        <location filename="../src/core/news.cpp" line="+85"/>
        <source>Arch Linux news</source>
        <translation>Arch-Linux-Neuigkeiten</translation>
    </message>
    <message>
        <location filename="../src/core/notices.cpp" line="+25"/>
        <source>%1 and %2</source>
        <translation>%1 und %2</translation>
    </message>
    <message>
        <location line="+32"/>
        <source>the kernel</source>
        <translation>der Kernel</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>the NVIDIA driver</source>
        <translation>der NVIDIA-Treiber</translation>
    </message>
    <message>
        <location line="+48"/>
        <source>including %1</source>
        <translation>darunter %1</translation>
    </message>
    <message>
        <location filename="../src/core/statustext.cpp" line="+25"/>
        <source>yesterday %1</source>
        <translation>gestern %1</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>%1 check failed at %2: %3</source>
        <translation>Prüfung von %1 fehlgeschlagen (%2): %3</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>; list from %1</source>
        <translation>; Liste von %1</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>Cannot read the update list: %1</source>
        <translation>Die Liste der Aktualisierungen kann nicht gelesen werden: %1</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+5"/>
        <source>Checking for updates</source>
        <translation>Wird auf Aktualisierungen geprüft</translation>
    </message>
    <message>
        <location line="-5"/>
        <location filename="../src/core/traystate.cpp" line="+558"/>
        <source>Not checked yet</source>
        <translation>Noch nicht geprüft</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Checked %1</source>
        <translation>Geprüft: %1</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>AUR not checked since %1: %2</source>
        <translation>AUR seit %1 nicht geprüft: %2</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>AUR check failed: %1</source>
        <translation>Prüfung des AUR fehlgeschlagen: %1</translation>
    </message>
    <message>
        <location filename="../src/core/traystate.cpp" line="-495"/>
        <source>AUR: %1 not updated last time; offered again with a newer version</source>
        <translation>AUR: %1 beim letzten Mal nicht aktualisiert; wird mit einer neueren Version erneut angeboten</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>1 configuration file to merge: %1</source>
        <translation>1 Konfigurationsdatei zusammenzuführen: %1</translation>
    </message>
    <message numerus="yes">
        <location line="+1"/>
        <location line="+465"/>
        <source>%n configuration files to merge</source>
        <comment>1 configuration file to merge</comment>
        <translation>
            <numerusform>%n Konfigurationsdatei zusammenzuführen</numerusform>
            <numerusform>%n Konfigurationsdateien zusammenzuführen</numerusform>
        </translation>
    </message>
    <message>
        <location line="-366"/>
        <source>Merging configuration files</source>
        <translation>Konfigurationsdateien werden zusammengeführt</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Cleaning up</source>
        <translation>Wird aufgeräumt</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Updating</source>
        <translation>Wird aktualisiert</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>The last update did not finish</source>
        <translation>Die letzte Aktualisierung wurde nicht abgeschlossen</translation>
    </message>
    <message>
        <location line="+9"/>
        <location filename="../src/update/main.cpp" line="+1367"/>
        <source>Restart required: %1</source>
        <translation>Neustart erforderlich: %1</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+326"/>
        <source>Read first: %1</source>
        <translation>Zuerst lesen: %1</translation>
    </message>
    <message>
        <location line="-285"/>
        <source>No network connection.</source>
        <translation>Keine Netzwerkverbindung.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Flatpak&apos;s server did not answer.</source>
        <translation>Der Flatpak-Server hat nicht geantwortet.</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>The mirrors did not answer.</source>
        <translation>Die Spiegelserver haben nicht geantwortet.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 reported an error.</source>
        <translation>%1 hat einen Fehler gemeldet.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>The check did not finish.</source>
        <translation>Die Prüfung wurde nicht abgeschlossen.</translation>
    </message>
    <message>
        <location line="+62"/>
        <source>Download Size</source>
        <translation>Download-Größe</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Package (%1)</source>
        <translation>Paket (%1)</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Old Version</source>
        <translation>Alte Version</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>New Version</source>
        <translation>Neue Version</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Branch</source>
        <translation>Zweig</translation>
    </message>
    <message numerus="yes">
        <location line="+47"/>
        <source>%n updates available</source>
        <comment>1 update available</comment>
        <translation>
            <numerusform>%n Aktualisierung verfügbar</numerusform>
            <numerusform>%n Aktualisierungen verfügbar</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location line="+7"/>
        <location filename="../src/update/main.cpp" line="-824"/>
        <location line="+780"/>
        <source>%n packages</source>
        <comment>1 package</comment>
        <translation>
            <numerusform>%n Paket</numerusform>
            <numerusform>%n Pakete</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location line="+6"/>
        <location filename="../src/update/main.cpp" line="-774"/>
        <location line="+780"/>
        <source>%n Flatpaks</source>
        <comment>1 Flatpak</comment>
        <translation>
            <numerusform>%n Flatpak</numerusform>
            <numerusform>%n Flatpaks</numerusform>
        </translation>
    </message>
    <message>
        <location line="+14"/>
        <location line="+124"/>
        <source>Checking…</source>
        <translation>Wird geprüft …</translation>
    </message>
    <message>
        <location line="-113"/>
        <source>Merging configuration files…</source>
        <translation>Konfigurationsdateien werden zusammengeführt …</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>pacdiff is running in the Kyu window.</source>
        <translation>pacdiff läuft im Kyu-Fenster.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Cleaning up…</source>
        <translation>Wird aufgeräumt …</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Updating…</source>
        <translation>Wird aktualisiert …</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Running in the Kyu window.</source>
        <translation>Läuft im Kyu-Fenster.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Show Window</source>
        <translation>Fenster anzeigen</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Update failed</source>
        <translation>Aktualisierung fehlgeschlagen</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>The update was interrupted.</source>
        <translation>Die Aktualisierung wurde unterbrochen.</translation>
    </message>
    <message numerus="yes">
        <location line="+1"/>
        <source>Only %n packages were upgraded.</source>
        <comment>Only 1 package was upgraded.</comment>
        <translation>
            <numerusform>Nur %n Paket wurde aktualisiert.</numerusform>
            <numerusform>Nur %n Pakete wurden aktualisiert.</numerusform>
        </translation>
    </message>
    <message>
        <location line="+1"/>
        <location filename="../src/update/main.cpp" line="-106"/>
        <source>No packages were upgraded.</source>
        <translation>Es wurden keine Pakete aktualisiert.</translation>
    </message>
    <message>
        <location line="+10"/>
        <location line="+16"/>
        <location line="+63"/>
        <source>Try Again</source>
        <translation>Erneut versuchen</translation>
    </message>
    <message>
        <location line="-74"/>
        <source>Could not check for updates</source>
        <translation>Prüfung auf Aktualisierungen fehlgeschlagen</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Kyu could not read its list of updates.</source>
        <translation>Kyu konnte seine Liste der Aktualisierungen nicht lesen.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Last successful check: %1</source>
        <translation>Letzte erfolgreiche Prüfung: %1</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Restart required</source>
        <translation>Neustart erforderlich</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Kernel %1 is installed.</source>
        <translation>Kernel %1 ist installiert.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>The running kernel %1 was removed.</source>
        <translation>Der laufende Kernel %1 wurde entfernt.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>The running kernel was replaced.</source>
        <translation>Der laufende Kernel wurde ersetzt.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>An update of %1 is installed.</source>
        <translation>Eine Aktualisierung von %1 ist installiert.</translation>
    </message>
    <message>
        <location line="+10"/>
        <location line="+75"/>
        <source>Merge…</source>
        <translation>Zusammenführen …</translation>
    </message>
    <message>
        <location line="-72"/>
        <source>Restart</source>
        <translation>Neu starten</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>%1 download</source>
        <translation>%1 Download</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>%1 AUR held back</source>
        <translation>%1 AUR zurückgehalten</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Could not check for updates · list from %1</source>
        <translation>Prüfung auf Aktualisierungen fehlgeschlagen · Liste von %1</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Could not check for updates · the last list</source>
        <translation>Prüfung auf Aktualisierungen fehlgeschlagen · die letzte Liste</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Updates the running kernel · requires a restart</source>
        <translation>Aktualisiert den laufenden Kernel · Neustart erforderlich</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>News: %1</source>
        <translation>Neuigkeiten: %1</translation>
    </message>
    <message>
        <location filename="../src/card/cardwindow.cpp" line="+299"/>
        <source>Details</source>
        <translation>Details</translation>
    </message>
    <message>
        <location line="+20"/>
        <location filename="../src/core/traystate.cpp" line="+6"/>
        <location filename="../src/tray/trayicon.cpp" line="+268"/>
        <source>Update</source>
        <translation>Aktualisieren</translation>
    </message>
    <message>
        <location line="+376"/>
        <source>Restart now? Unsaved work in open programs is lost.</source>
        <translation>Jetzt neu starten? Nicht gespeicherte Arbeit in geöffneten Programmen geht verloren.</translation>
    </message>
    <message numerus="yes">
        <location filename="../src/core/traystate.cpp" line="+14"/>
        <source>%1 and %n more</source>
        <comment>%1 and 1 more</comment>
        <translation>
            <numerusform>%1 und %n weitere</numerusform>
            <numerusform>%1 und %n weitere</numerusform>
        </translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Your files stay in use until you merge.</source>
        <translation>Bis zum Zusammenführen bleiben die bisherigen Dateien in Gebrauch.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Your old file: %1</source>
        <translation>Bisherige Datei: %1</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>The package&apos;s version is in use now.</source>
        <translation>Die Version aus dem Paket ist jetzt in Gebrauch.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>New version: %1</source>
        <translation>Neue Version: %1</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Your file stays in use until you merge.</source>
        <translation>Bis zum Zusammenführen bleibt die bisherige Datei in Gebrauch.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Checking for updates…</source>
        <translation>Wird auf Aktualisierungen geprüft …</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+13"/>
        <source>Check for Updates</source>
        <translation>Auf Aktualisierungen prüfen</translation>
    </message>
    <message>
        <location line="-7"/>
        <source>Up to date</source>
        <translation>Auf dem neuesten Stand</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Last checked: %1</source>
        <translation>Zuletzt geprüft: %1</translation>
    </message>
    <message>
        <location filename="../src/core/updatestate.cpp" line="+59"/>
        <source>No updates</source>
        <translation>Keine Aktualisierungen</translation>
    </message>
    <message numerus="yes">
        <location line="+2"/>
        <source>%n updates</source>
        <comment>1 update</comment>
        <translation>
            <numerusform>%n Aktualisierung</numerusform>
            <numerusform>%n Aktualisierungen</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../src/core/waiting.cpp" line="+81"/>
        <source>An answer is needed in the Kyu window.</source>
        <translation>Im Kyu-Fenster wird eine Antwort benötigt.</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Something to read in the Kyu window.</source>
        <translation>Im Kyu-Fenster gibt es etwas zu lesen.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Your confirmation is needed.</source>
        <translation>Eine Bestätigung wird benötigt.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Your password is needed.</source>
        <translation>Das Passwort wird benötigt.</translation>
    </message>
    <message>
        <location filename="../src/card/main.cpp" line="+125"/>
        <source>What is going on with updates, and the one thing to do next.</source>
        <translation>Zeigt den Stand der Aktualisierungen und den nächsten Schritt.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Show a sample state (%1), for screenshots.</source>
        <translation>Einen Beispielzustand anzeigen (%1), für Bildschirmfotos.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>name</source>
        <translation>Name</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Save the card to this PNG file and quit.</source>
        <translation>Die Karte in dieser PNG-Datei speichern und beenden.</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>file</source>
        <translation>Datei</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>With --demo: Details open.</source>
        <translation>Mit --demo: Details geöffnet.</translation>
    </message>
    <message>
        <location filename="../src/checker/main.cpp" line="+53"/>
        <source>Checks for pending pacman, AUR and Flatpak updates and writes them to the Kyu state file.</source>
        <translation>Prüft auf ausstehende Aktualisierungen von pacman, AUR und Flatpak und schreibt sie in die Kyu-Statusdatei.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Do not check repo packages.</source>
        <translation>Pakete aus den Paketquellen nicht prüfen.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Do not check Flatpaks.</source>
        <translation>Flatpaks nicht prüfen.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Print the resulting state as JSON.</source>
        <translation>Den resultierenden Zustand als JSON ausgeben.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>State file to update (default: %1).</source>
        <translation>Zu aktualisierende Statusdatei (Standard: %1).</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+2"/>
        <location line="+1"/>
        <location line="+6"/>
        <location line="+4"/>
        <location line="+2"/>
        <location line="+2"/>
        <location line="+8"/>
        <source>path</source>
        <translation>Pfad</translation>
    </message>
    <message>
        <location line="-24"/>
        <source>checkupdates binary.</source>
        <translation>Programmdatei von checkupdates.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>pacman binary.</source>
        <translation>Programmdatei von pacman.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Do not check AUR packages.</source>
        <translation>AUR-Pakete nicht prüfen.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>AUR helper (paru, yay, pamac), or none.</source>
        <translation>AUR-Hilfsprogramm (paru, yay, pamac) oder „none“.</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>program</source>
        <translation>Programm</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>AUR info URL.</source>
        <translation>URL der AUR-Info-Schnittstelle.</translation>
    </message>
    <message>
        <location line="+0"/>
        <location line="+5"/>
        <source>url</source>
        <translation>URL</translation>
    </message>
    <message>
        <location line="-4"/>
        <source>Ask the AUR now, even if it was asked in the last hours.</source>
        <translation>Das AUR jetzt abfragen, auch wenn es in den letzten Stunden abgefragt wurde.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>pacman-conf binary.</source>
        <translation>Programmdatei von pacman-conf.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Do not fetch the distro&apos;s news.</source>
        <translation>Die Neuigkeiten der Distribution nicht abrufen.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>News feed URL.</source>
        <translation>URL des Neuigkeiten-Feeds.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>os-release file.</source>
        <translation>os-release-Datei.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>pacman log.</source>
        <translation>pacman-Protokoll.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Kernel modules directory.</source>
        <translation>Ordner der Kernel-Module.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Running kernel release.</source>
        <translation>Version des laufenden Kernels.</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>release</source>
        <translation>Version</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Database copy for checkupdates (default: %1).</source>
        <translation>Datenbankkopie für checkupdates (Standard: %1).</translation>
    </message>
    <message>
        <location line="+71"/>
        <source>News</source>
        <translation>Neuigkeiten</translation>
    </message>
    <message>
        <location filename="../src/tray/trayicon.cpp" line="-210"/>
        <location line="+171"/>
        <location line="+42"/>
        <source>Show updates</source>
        <translation>Aktualisierungen anzeigen</translation>
    </message>
    <message>
        <location line="-209"/>
        <source>Check for updates now</source>
        <translation>Jetzt auf Aktualisierungen prüfen</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Updates</source>
        <translation>Aktualisierungen</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Quit</source>
        <translation>Beenden</translation>
    </message>
    <message>
        <location line="+142"/>
        <source>Read before updating</source>
        <translation>Vor dem Aktualisieren lesen</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Read</source>
        <translation>Lesen</translation>
    </message>
    <message>
        <location filename="../src/update/console.h" line="+131"/>
        <source>Fix</source>
        <translation>Lösung</translation>
    </message>
    <message>
        <location filename="../src/update/console.cpp" line="+429"/>
        <source>%1 s</source>
        <translation>%1 s</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 min</source>
        <translation>%1 min</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Updating packages</source>
        <translation>Pakete werden aktualisiert</translation>
    </message>
    <message>
        <location line="+66"/>
        <source>pacdiff is in pacman-contrib: %1</source>
        <translation>pacdiff ist in pacman-contrib enthalten: %1</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>pacdiff needs sudo, doas or pkexec to change files in /etc.</source>
        <translation>pacdiff benötigt sudo, doas oder pkexec, um Dateien in /etc zu ändern.</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>Differences are shown with %1 (set DIFFPROG to change).</source>
        <translation>Unterschiede werden mit %1 angezeigt (mit DIFFPROG änderbar).</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>pacdiff asks for each file: v view the differences, m merge, r remove the new version, o use the new version, s skip.</source>
        <extracomment>v, m, r, o and s are pacdiff&apos;s own keys: keep them.</extracomment>
        <translation>pacdiff fragt bei jeder Datei: v Unterschiede ansehen, m zusammenführen, r neue Version entfernen, o neue Version verwenden, s überspringen.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>pacdiff stopped (status %1).</source>
        <translation>pacdiff ist abgebrochen (Status %1).</translation>
    </message>
    <message>
        <location filename="../src/update/main.cpp" line="-1152"/>
        <source>Can be cleaned up, if you like:</source>
        <translation>Kann bei Bedarf aufgeräumt werden:</translation>
    </message>
    <message numerus="yes">
        <location line="+2"/>
        <source>%n orphan packages: %1</source>
        <comment>1 orphan package: %1</comment>
        <translation>
            <numerusform>%n verwaistes Paket: %1</numerusform>
            <numerusform>%n verwaiste Pakete: %1</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location line="+3"/>
        <source>(%n packages in all, with what only they need)</source>
        <comment>(1 package in all, with what only they need)</comment>
        <translation>
            <numerusform>(insgesamt %n Paket, samt dem, was nur sie benötigen)</numerusform>
            <numerusform>(insgesamt %n Pakete, samt dem, was nur sie benötigen)</numerusform>
        </translation>
    </message>
    <message>
        <location line="+5"/>
        <source>%1 of older and removed packages in the package cache</source>
        <translation>%1 an älteren und entfernten Paketen im Paket-Cache</translation>
    </message>
    <message numerus="yes">
        <location line="+3"/>
        <source>%n Flatpak runtimes no app uses</source>
        <comment>1 Flatpak runtime no app uses</comment>
        <translation>
            <numerusform>%n Flatpak-Laufzeitumgebung, die keine Anwendung verwendet</numerusform>
            <numerusform>%n Flatpak-Laufzeitumgebungen, die keine Anwendung verwendet</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../src/update/console.cpp" line="-534"/>
        <location filename="../src/update/main.cpp" line="+16"/>
        <source>Orphans</source>
        <translation>Verwaist</translation>
    </message>
    <message>
        <location filename="../src/update/main.cpp" line="+2"/>
        <source>Installed as dependencies and needed by nothing now. pacman asks first; to keep one, answer n and run:</source>
        <extracomment>&quot;n&quot; is the answer to pacman&apos;s question: keep it.</extracomment>
        <translation>Als Abhängigkeiten installiert und jetzt von nichts mehr benötigt. pacman fragt zuerst; um eines zu behalten, mit n antworten und Folgendes ausführen:</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Orphans kept.</source>
        <translation>Die verwaisten Pakete bleiben installiert.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>The orphans were not removed (status %1), see above.</source>
        <translation>Die verwaisten Pakete wurden nicht entfernt (Status %1), siehe oben.</translation>
    </message>
    <message>
        <location filename="../src/update/console.cpp" line="+0"/>
        <location filename="../src/update/main.cpp" line="+6"/>
        <source>Cache</source>
        <translation>Cache</translation>
    </message>
    <message>
        <location filename="../src/update/main.cpp" line="+0"/>
        <source>%1, then %2</source>
        <translation>%1, dann %2</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Remove %1 from the package cache: older versions (the newest %2 of each stay) and packages no longer installed? [y/N] </source>
        <extracomment>Keep [y/N]: Kyu reads &quot;y&quot; as yes.</extracomment>
        <translation>%1 aus dem Paket-Cache entfernen: ältere Versionen (die neuesten %2 jedes Pakets bleiben) und nicht mehr installierte Pakete? [y/N] </translation>
    </message>
    <message>
        <location line="+13"/>
        <source>The package cache was not cleaned (status %1), see above.</source>
        <translation>Der Paket-Cache wurde nicht bereinigt (Status %1), siehe oben.</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>The unused runtimes were not removed (status %1), see above.</source>
        <translation>Die unbenutzten Laufzeitumgebungen wurden nicht entfernt (Status %1), siehe oben.</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Cleaned up.</source>
        <translation>Aufgeräumt.</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>Updates the system in this terminal: %1, then %2, then the AUR helper&apos;s update, with the news before and what is left to do after.</source>
        <translation>Aktualisiert das System in diesem Terminal: %1, dann %2, dann die Aktualisierung des AUR-Hilfsprogramms, mit den Neuigkeiten davor und dem, was danach noch zu tun ist.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Do not wait for a key at the end.</source>
        <translation>Am Ende nicht auf einen Tastendruck warten.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Do not run kyu-check afterwards.</source>
        <translation>kyu-check danach nicht ausführen.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Download all package databases again (pacman -Syyu), as after changing mirrors.</source>
        <translation>Alle Paketdatenbanken erneut herunterladen (pacman -Syyu), wie nach einem Wechsel der Spiegelserver.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Only merge the configuration files the last update left (pacdiff).</source>
        <translation>Nur die Konfigurationsdateien der letzten Aktualisierung zusammenführen (pacdiff).</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Print what installing and removing the queue in FILE would do, as JSON (no root).</source>
        <translation>Gibt aus, was das Installieren und Entfernen der Liste in DATEI tun würde, als JSON (ohne root).</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Install and remove what the queue in FILE asks for, step by step with pacman.</source>
        <translation>Installiert und entfernt, was die Liste in DATEI verlangt, Schritt für Schritt mit pacman.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Only offer to clean up: orphan packages, the package cache, unused Flatpak runtimes.</source>
        <translation>Nur das Aufräumen anbieten: verwaiste Pakete, Paket-Cache, unbenutzte Flatpak-Laufzeitumgebungen.</translation>
    </message>
    <message>
        <location line="+76"/>
        <source>[f] fix and try again</source>
        <extracomment>Keep the key in brackets as it is: Kyu reads that letter.</extracomment>
        <translation>[f] beheben und erneut versuchen</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>[r] restart</source>
        <extracomment>Keep the key in brackets as it is: Kyu reads that letter.</extracomment>
        <translation>[r] neu starten</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>[m] merge</source>
        <extracomment>Keep the key in brackets as it is: Kyu reads that letter.</extracomment>
        <translation>[m] zusammenführen</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>[c] clean up</source>
        <extracomment>Keep the key in brackets as it is: Kyu reads that letter.</extracomment>
        <translation>[c] aufräumen</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+96"/>
        <source>[Enter] close</source>
        <extracomment>The Enter key</extracomment>
        <translation>[Enter] schließen</translation>
    </message>
    <message>
        <location line="-76"/>
        <source>Plasma asks whether to restart now.</source>
        <translation>Plasma fragt, ob jetzt neu gestartet werden soll.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Restart now? Unsaved work in open programs is lost. [y/N] </source>
        <extracomment>Keep [y/N]: Kyu reads &quot;y&quot; as yes.</extracomment>
        <translation>Jetzt neu starten? Nicht gespeicherte Arbeit in geöffneten Programmen geht verloren. [y/N] </translation>
    </message>
    <message>
        <location line="+8"/>
        <source>systemctl reboot failed (status %1).</source>
        <translation>systemctl reboot ist fehlgeschlagen (Status %1).</translation>
    </message>
    <message>
        <location line="+16"/>
        <location line="+149"/>
        <source>Configuration files merged.</source>
        <translation>Konfigurationsdateien zusammengeführt.</translation>
    </message>
    <message>
        <location line="-139"/>
        <source>The fix failed (status %1); its messages are above.</source>
        <translation>Die Behebung ist fehlgeschlagen (Status %1); die Meldungen stehen oben.</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>Could not start kyu-update again: run it once more yourself.</source>
        <translation>kyu-update konnte nicht erneut gestartet werden: Es muss noch einmal von Hand ausgeführt werden.</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>An update is already running (pid %1).</source>
        <translation>Es läuft bereits eine Aktualisierung (PID %1).</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>An update is already running.</source>
        <translation>Es läuft bereits eine Aktualisierung.</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>Kyu · installing and removing</source>
        <translation>Kyu · Installieren und Entfernen</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>The list to install and remove cannot be read: %1</source>
        <translation>Die Liste zum Installieren und Entfernen kann nicht gelesen werden: %1</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Neither sudo, doas nor pkexec is installed: pacman cannot change the system.</source>
        <translation>Weder sudo noch doas noch pkexec ist installiert: pacman kann das System nicht ändern.</translation>
    </message>
    <message numerus="yes">
        <location line="+12"/>
        <source>Installing needs the %n pending updates first.</source>
        <comment>Installing needs the pending update first.</comment>
        <translation>
            <numerusform>Für die Installation wird zuerst die ausstehende Aktualisierung benötigt.</numerusform>
            <numerusform>Für die Installation werden zuerst die %n ausstehenden Aktualisierungen benötigt.</numerusform>
        </translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Installing needs newer libraries than the system has: the system is updated first.</source>
        <translation>Für die Installation werden neuere Bibliotheken benötigt, als das System hat: Zuerst wird das System aktualisiert.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Update now, then install? [y/N] </source>
        <translation>Jetzt aktualisieren, dann installieren? [y/N] </translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Nothing was changed.</source>
        <translation>Es wurde nichts geändert.</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Kyu · clean up</source>
        <translation>Kyu · Aufräumen</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Nothing to clean up.</source>
        <translation>Nichts aufzuräumen.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Kyu · configuration files</source>
        <translation>Kyu · Konfigurationsdateien</translation>
    </message>
    <message numerus="yes">
        <location line="+22"/>
        <source>%n configuration files still to merge.</source>
        <comment>1 configuration file still to merge.</comment>
        <translation>
            <numerusform>%n Konfigurationsdatei ist noch zusammenzuführen.</numerusform>
            <numerusform>%n Konfigurationsdateien sind noch zusammenzuführen.</numerusform>
        </translation>
    </message>
    <message>
        <location line="+26"/>
        <source>Kyu · updating</source>
        <translation>Kyu · Aktualisierung</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+780"/>
        <source>%1 AUR</source>
        <translation>%1 AUR</translation>
    </message>
    <message numerus="yes">
        <location line="-775"/>
        <source>Kyu · %n updates: %1</source>
        <comment>Kyu · 1 update: %1</comment>
        <translation>
            <numerusform>Kyu · %n Aktualisierung: %1</numerusform>
            <numerusform>Kyu · %n Aktualisierungen: %1</numerusform>
        </translation>
    </message>
    <message>
        <location line="+3"/>
        <source>AUR, held back: %1 (not built last time; tried again when the AUR has a newer version).</source>
        <translation>AUR, zurückgehalten: %1 (beim letzten Mal nicht gebaut; erneuter Versuch, sobald das AUR eine neuere Version hat).</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Keep this window open until Kyu says it is done.</source>
        <translation>Dieses Fenster geöffnet lassen, bis Kyu meldet, dass alles erledigt ist.</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>News (read): %1</source>
        <translation>Neuigkeiten (gelesen): %1</translation>
    </message>
    <message numerus="yes">
        <location line="+1"/>
        <source>+%n more</source>
        <comment>+1 more</comment>
        <translation>
            <numerusform>+%n weitere</numerusform>
            <numerusform>+%n weitere</numerusform>
        </translation>
    </message>
    <message>
        <location line="+4"/>
        <source>%1 since your last update</source>
        <translation>%1 seit der letzten Aktualisierung</translation>
    </message>
    <message numerus="yes">
        <location line="+6"/>
        <source>… and %n more: %1</source>
        <comment>… and 1 more: %1</comment>
        <translation>
            <numerusform>… und %n weitere: %1</numerusform>
            <numerusform>… und %n weitere: %1</numerusform>
        </translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Continue? [y/N] </source>
        <extracomment>Keep [y/N]: Kyu reads &quot;y&quot; as yes.</extracomment>
        <translation>Fortfahren? [y/N] </translation>
    </message>
    <message>
        <location line="+2"/>
        <source>News to read first: continue? [y/N]</source>
        <extracomment>Keep [y/N]: Kyu reads &quot;y&quot; as yes.</extracomment>
        <translation>Ungelesene Neuigkeiten: Fortfahren? [y/N]</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+178"/>
        <location line="+147"/>
        <source>Stopped. No packages were upgraded.</source>
        <translation>Abgebrochen. Es wurden keine Pakete aktualisiert.</translation>
    </message>
    <message>
        <location filename="../src/update/apply.cpp" line="+100"/>
        <source>Waiting for pacman&apos;s lock to go (%1).</source>
        <translation>Es wird gewartet, bis die pacman-Sperre aufgehoben ist (%1).</translation>
    </message>
    <message>
        <location line="+1"/>
        <location filename="../src/update/main.cpp" line="-274"/>
        <source>Waiting for %1 to finish. Press Ctrl+C to cancel.</source>
        <translation>Es wird gewartet, bis %1 fertig ist. Zum Abbrechen Strg+C drücken.</translation>
    </message>
    <message>
        <location line="+66"/>
        <source>A package file changed since it was added: %1. Add it again if you still want it.</source>
        <translation>Eine Paketdatei hat sich seit dem Hinzufügen geändert: %1. Bei Bedarf erneut hinzufügen.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Planning</source>
        <translation>Planung</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>refreshing the package lists (no root)</source>
        <translation>Paketlisten werden aufgefrischt (ohne root)</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>The package lists could not be refreshed (%1).</source>
        <translation>Die Paketlisten konnten nicht aufgefrischt werden (%1).</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Nothing to do.</source>
        <translation>Nichts zu tun.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Installing needs the network. Removing can go ahead without it.</source>
        <translation>Zum Installieren wird das Netzwerk benötigt. Entfernen ist auch ohne möglich.</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>Step %1 of %2: %3</source>
        <translation>Schritt %1 von %2: %3</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Stopped before %1.</source>
        <translation>Vor „%1“ angehalten.</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>pacman&apos;s list differs from the plan (the package lists changed):</source>
        <translation>Die Liste von pacman weicht vom Plan ab (die Paketlisten haben sich geändert):</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>also: %1</source>
        <translation>zusätzlich: %1</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>no longer: %1</source>
        <translation>nicht mehr: %1</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Continue with pacman&apos;s list? [y/N] </source>
        <translation>Mit der Liste von pacman fortfahren? [y/N] </translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Stopped. Nothing of this step was changed.</source>
        <translation>Angehalten. Von diesem Schritt wurde nichts geändert.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>pacman</source>
        <translation>pacman</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Stopped: %1 was not done.</source>
        <translation>Angehalten: „%1“ wurde nicht ausgeführt.</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>%1 failed (status %2); pacman&apos;s messages are above.</source>
        <translation>„%1“ ist fehlgeschlagen (Status %2); die pacman-Meldungen stehen oben.</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Not done, still in Pending: %1</source>
        <translation>Nicht ausgeführt, weiterhin vorgemerkt: %1</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Done.</source>
        <translation>Fertig.</translation>
    </message>
    <message>
        <location filename="../src/update/main.cpp" line="+65"/>
        <source>waited %1</source>
        <translation>gewartet: %1</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>pacman&apos;s lock looks left over: %1 is running but idle.</source>
        <translation>Die pacman-Sperre ist offenbar verwaist: %1 läuft, ist aber untätig.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>pacman&apos;s lock is there; Kyu cannot see other users&apos; programs to tell who has it.</source>
        <translation>Die pacman-Sperre ist vorhanden; Kyu kann die Programme anderer Benutzer nicht sehen, um festzustellen, wer sie hält.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>pacman&apos;s lock is left over from a stopped run; nothing is using it.</source>
        <translation>Die pacman-Sperre stammt von einem abgebrochenen Lauf; nichts verwendet sie.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Fix (if no pacman is running)</source>
        <translation>Lösung (wenn kein pacman läuft)</translation>
    </message>
    <message>
        <location line="+12"/>
        <location line="+8"/>
        <source>%1 is working now and may be using the lock: wait until it is done.</source>
        <translation>%1 arbeitet gerade und verwendet möglicherweise die Sperre: Warten, bis es fertig ist.</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>Still busy after %1. Update when it is done.</source>
        <translation>Nach %1 immer noch beschäftigt. Aktualisieren, wenn es fertig ist.</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Neither sudo, doas nor pkexec is installed: pacman needs one of them to run as root.</source>
        <translation>Weder sudo noch doas noch pkexec ist installiert: pacman benötigt eines davon, um als root zu laufen.</translation>
    </message>
    <message>
        <location line="+135"/>
        <source>Stopped. Only %1 upgraded.</source>
        <translation>Abgebrochen. Nur %1 aktualisiert.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>(again)</source>
        <translation>(erneut)</translation>
    </message>
    <message>
        <location filename="../src/update/console.cpp" line="+1"/>
        <location filename="../src/update/main.cpp" line="+24"/>
        <source>Snapshot</source>
        <translation>Snapshot</translation>
    </message>
    <message>
        <location filename="../src/update/main.cpp" line="+1"/>
        <source>%1 takes one first</source>
        <translation>%1 legt zuerst einen an</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>%1 takes one before the keyrings and one before the rest</source>
        <translation>%1 legt einen vor den Schlüsselbunden und einen vor dem Rest an</translation>
    </message>
    <message>
        <location filename="../src/update/console.cpp" line="-2"/>
        <location filename="../src/update/main.cpp" line="+7"/>
        <source>Keyrings</source>
        <translation>Schlüsselbunde</translation>
    </message>
    <message>
        <location line="+0"/>
        <location filename="../src/update/main.cpp" line="+3"/>
        <location line="+2"/>
        <source>Packages</source>
        <translation>Pakete</translation>
    </message>
    <message>
        <location filename="../src/update/main.cpp" line="+163"/>
        <source>%1 asks its own questions, and may ask for the password again.</source>
        <translation>%1 stellt eigene Fragen und fragt möglicherweise erneut nach dem Passwort.</translation>
    </message>
    <message>
        <location line="+52"/>
        <source>Flatpaks and AUR packages were skipped.</source>
        <translation>Flatpaks und AUR-Pakete wurden übersprungen.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Flatpaks were skipped.</source>
        <translation>Flatpaks wurden übersprungen.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>AUR packages were skipped.</source>
        <translation>AUR-Pakete wurden übersprungen.</translation>
    </message>
    <message numerus="yes">
        <location line="+30"/>
        <source>Only %1 were upgraded.</source>
        <comment>Only %1 was upgraded.</comment>
        <translation>
            <numerusform>Nur %1 wurde aktualisiert.</numerusform>
            <numerusform>Nur %1 wurden aktualisiert.</numerusform>
        </translation>
    </message>
    <message>
        <location line="+3"/>
        <source>The update was interrupted while pacman was changing packages.</source>
        <translation>Die Aktualisierung wurde unterbrochen, während pacman Pakete änderte.</translation>
    </message>
    <message numerus="yes">
        <location line="+1"/>
        <source>The update was interrupted after %n packages had changed.</source>
        <comment>The update was interrupted after 1 package had changed.</comment>
        <translation>
            <numerusform>Die Aktualisierung wurde unterbrochen, nachdem sich %n Paket geändert hatte.</numerusform>
            <numerusform>Die Aktualisierung wurde unterbrochen, nachdem sich %n Pakete geändert hatten.</numerusform>
        </translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Run the update again: pacman completes it.</source>
        <translation>Die Aktualisierung erneut ausführen: pacman schließt sie ab.</translation>
    </message>
    <message numerus="yes">
        <location line="+2"/>
        <source>pacman failed after changing %n packages.</source>
        <comment>pacman failed after changing 1 package.</comment>
        <translation>
            <numerusform>pacman ist fehlgeschlagen, nachdem %n Paket geändert wurde.</numerusform>
            <numerusform>pacman ist fehlgeschlagen, nachdem %n Pakete geändert wurden.</numerusform>
        </translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Stopped.</source>
        <translation>Abgebrochen.</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>pacman failed.</source>
        <translation>pacman ist fehlgeschlagen.</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>Check that the clock is right, then reload and refresh pacman&apos;s keys: %1, then %2 (slow). pacman&apos;s messages above name the key.</source>
        <translation>Prüfen, ob die Uhrzeit stimmt, dann die pacman-Schlüssel neu laden und auffrischen: %1, dann %2 (langsam). Die pacman-Meldungen oben nennen den Schlüssel.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>The new keyring itself could not be checked: the installed one is too old to trust it, which happens after a long gap.</source>
        <translation>Der neue Schlüsselbund selbst konnte nicht geprüft werden: Der installierte ist zu alt, um dem neuen zu vertrauen; das kommt nach einer langen Pause vor.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>The keyrings are already current, so a newer keyring is not the answer.</source>
        <translation>Die Schlüsselbunde sind bereits aktuell, ein neuerer Schlüsselbund ist also nicht die Lösung.</translation>
    </message>
    <message numerus="yes">
        <location line="+10"/>
        <source>and %n more</source>
        <comment>and 1 more</comment>
        <translation>
            <numerusform>und %n weitere</numerusform>
            <numerusform>und %n weitere</numerusform>
        </translation>
    </message>
    <message>
        <location line="+26"/>
        <source>pacman&apos;s last lines (status %1):</source>
        <translation>Die letzten Zeilen von pacman (Status %1):</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>The package lists are newer than the system: update before installing anything.</source>
        <translation>Die Paketlisten sind neuer als das System: Vor dem Installieren weiterer Pakete erst aktualisieren.</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>Done</source>
        <translation>Fertig</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Done in %1</source>
        <translation>Fertig nach %1</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>The system is up to date.</source>
        <translation>Das System ist auf dem neuesten Stand.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Flatpaks: stopped. None were updated.</source>
        <translation>Flatpaks: abgebrochen. Es wurden keine aktualisiert.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Flatpaks failed (status %1), see above.</source>
        <translation>Flatpaks fehlgeschlagen (Status %1), siehe oben.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Flatpaks not updated: flatpak found nothing to update (a remote it could not reach?), see above.</source>
        <translation>Flatpaks nicht aktualisiert: flatpak hat nichts zu aktualisieren gefunden (eine nicht erreichbare Quelle?), siehe oben.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Already up to date.</source>
        <translation>Bereits auf dem neuesten Stand.</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>AUR packages not updated: %1 stopped (status %2), see above.</source>
        <translation>AUR-Pakete nicht aktualisiert: %1 ist abgebrochen (Status %2), siehe oben.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>The rest is done.</source>
        <translation>Der Rest ist erledigt.</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Do not restart: the update is incomplete. Run it again first.</source>
        <translation>Nicht neu starten: Die Aktualisierung ist unvollständig. Zuerst erneut ausführen.</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Restart required: %1 (from an earlier update)</source>
        <translation>Neustart erforderlich: %1 (von einer früheren Aktualisierung)</translation>
    </message>
    <message numerus="yes">
        <location line="+11"/>
        <source>%n configuration files to merge: %1</source>
        <comment>1 configuration file to merge: %1</comment>
        <translation>
            <numerusform>%n Konfigurationsdatei zusammenzuführen: %1</numerusform>
            <numerusform>%n Konfigurationsdateien zusammenzuführen: %1</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location line="+5"/>
        <source>%1 still use old libraries: restart them.</source>
        <comment>%1 still uses old libraries: restart it.</comment>
        <translation>
            <numerusform>%1 verwendet noch alte Bibliotheken und sollte neu gestartet werden.</numerusform>
            <numerusform>%1 verwenden noch alte Bibliotheken und sollten neu gestartet werden.</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../src/window/main.cpp" line="+138"/>
        <source>Stop the update?</source>
        <translation>Aktualisierung abbrechen?</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>The update is still running. Stopping it now can leave the system half updated.</source>
        <translation>Die Aktualisierung läuft noch. Ein Abbruch jetzt kann das System halb aktualisiert zurücklassen.</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Stop the update</source>
        <translation>Aktualisierung abbrechen</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Keep updating</source>
        <translation>Weiter aktualisieren</translation>
    </message>
    <message>
        <location filename="../src/window/waitingnotice.cpp" line="+51"/>
        <source>The update is waiting for you</source>
        <translation>Die Aktualisierung wartet auf eine Eingabe</translation>
    </message>
    <message>
        <location line="+6"/>
        <location line="+1"/>
        <source>Show</source>
        <translation>Anzeigen</translation>
    </message>
    <message>
        <location filename="../src/core/plan.cpp" line="+280"/>
        <source>%1 and %2 more</source>
        <translation>%1 und %2 weitere</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Install %1</source>
        <translation>%1 installieren</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Reinstall %1</source>
        <translation>%1 neu installieren</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Install %1 from a file</source>
        <translation>%1 aus einer Datei installieren</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Remove %1</source>
        <translation>%1 entfernen</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Mark %1 as installed as a dependency</source>
        <translation>%1 als Abhängigkeit markieren</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Mark %1 as explicitly installed</source>
        <translation>%1 als ausdrücklich installiert markieren</translation>
    </message>
    <message>
        <location line="+178"/>
        <source>%1 is already installed.</source>
        <translation>%1 ist bereits installiert.</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>%1 is in IgnorePkg (pacman.conf): pacman asks whether to install it anyway.</source>
        <translation>%1 steht in IgnorePkg (pacman.conf): pacman fragt, ob es trotzdem installiert werden soll.</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Not a package: %1. Choose the package itself (a group&apos;s members, or which package provides it).</source>
        <translation>Kein Paket: %1. Bitte das Paket selbst wählen (die Mitglieder einer Gruppe oder das Paket, das es bereitstellt).</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>%1 cannot be reinstalled: it is not installed from a repository.</source>
        <translation>%1 kann nicht neu installiert werden: Es stammt nicht aus einer Paketquelle.</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 has a newer version: updating installs it.</source>
        <translation>Für %1 gibt es eine neuere Version: Die Aktualisierung installiert sie.</translation>
    </message>
    <message>
        <location line="+25"/>
        <source>%1 is gone.</source>
        <translation>%1 ist nicht mehr vorhanden.</translation>
    </message>
    <message>
        <location line="+45"/>
        <location line="+49"/>
        <source>%1 is not installed.</source>
        <translation>%1 ist nicht installiert.</translation>
    </message>
    <message>
        <location line="-28"/>
        <source>pacman keeps %1 (HoldPkg): the system needs it.</source>
        <translation>pacman behält %1 (HoldPkg): Das System benötigt es.</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Other installed packages need it: %1.</source>
        <translation>Andere installierte Pakete benötigen es: %1.</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>The same packages are to be installed and removed: %1.</source>
        <translation>Dieselben Pakete sollen installiert und entfernt werden: %1.</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>%1 is marked so already.</source>
        <translation>%1 ist bereits so markiert.</translation>
    </message>
</context>
</TS>
