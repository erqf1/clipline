ERGEBNIS CLOUD-SESSION clipline-icon

1. Aufgabe und Status
   - Desktop-Symbol nicht mehr bei jedem Update neu anlegen: erledigt, Release v1.9.3 veröffentlicht.
   - Mehr echte Bilder für mert.win/clip: erledigt, 59 WebP-Bilder und Changelog-Schnipsel in website-updates/.

2. Branch, Commits, PR, Release, CI
   - Branch: claude/happy-albattani-k8ril0 (baut auf v1.9.2 = Branch claude/quirky-bell-peunm4 auf)
   - Commits: ebcb47b "Clipline 1.9.3: Desktop-Symbol nur bei der Erst-Installation, Website-Bilder", danach 763e20f, 7ffea41 und der letzte Commit (dieser Bericht)
   - PR: keiner neu angelegt (Push und Release aus der Cloud gingen). Offen ist weiter PR erqf1/clipline#1 (1.9.2) nach main:
     https://github.com/erqf1/clipline/pull/1 – main steht noch auf 1.9.1.
   - Release: v1.9.3 https://github.com/erqf1/clipline/releases/tag/v1.9.3, per workflow_dispatch mit Version 1.9.3 gestartet.
     Alle 7 Pakete drin: Windows setup.exe und zip, deb, tar.gz, Arch pkg.tar.zst, macOS Intel und Apple Silicon.
     Release-Notizen auf Deutsch mit Link auf https://mert.win/clip/changelog.html
   - CI: Release-Workflow komplett grün (Windows inkl. neuem Installer-Test, Linux, Arch, macOS x2, release).
     Linux test komplett grün: wayland, kde, siteshots, x11 (current), x11 (v1.7.0). Die X11-Jobs hingen im ersten Lauf
     bei apt-get (Paketserver) und liefen im zweiten Versuch durch: https://github.com/erqf1/clipline/actions/runs/44520432282
   - Damit sind alle CI-Jobs grün.

3. Was geändert wurde
   - packaging/windows/clipline.iss: [Code] erkennt eine vorherige Installation am Uninstall-Eintrag (HKLM64, HKLM32, HKCU).
     Task "desktopicon" und der Desktop-Eintrag haben "Check: IsFirstInstall". Bei Updates (auch still über den Updater)
     wird das Symbol weder angeboten noch angelegt. Beim Deinstallieren entfernt, weil es im Uninstall-Log der
     Erst-Installation steht. Gelöscht bleibt gelöscht.
   - .github/workflows/release.yml: neuer Schritt "Installer test" im Windows-Job: Erst-Installation mit Symbol, Update mit
     denselben Argumenten wie src/updater.cpp (Symbol unverändert, gleiche Änderungszeit), Deinstallation (Symbol weg),
     neu installieren, Symbol löschen, Update (bleibt weg). Release-Notizen jetzt aus .github/release-notes.md.
   - Linux (install.sh, deb, Arch) und macOS (dmg): legen kein Desktop-Symbol an, nur Menüeintrag bzw. App. Keine Änderung nötig.
   - Neuer Modus "clipline --siteshots <ordner>" (src/siteshots.cpp) plus tools/siteshots.sh: rendert alle Fenster unsichtbar
     (offscreen, zwei Bildschirme 2560×1440 und 1920×1080), Beispiel-Clips aus den eingebauten Fotos, Beispiel-Mikrofone und
     fester Pegel (nur in diesem Modus, über SiteShots-Hooks in setup.h/.cpp). Ausgabe als WebP.
   - .github/workflows/linux-test.yml: neuer Job "siteshots" (erzeugt die Bilder, lädt sie als Artifact hoch).
   - Version 1.9.3 (CMakeLists.txt).

4. Was getestet wurde
   - Windows-Installer: echter Test auf dem Windows-Runner (siehe oben), grün. Nicht getestet: Update von einer alten
     Installation (1.9.2 oder älter) auf 1.9.3 auf einem echten PC und Installation "für alle Benutzer" (HKLM).
   - Bilder: lokal (Ubuntu 24.04, Qt 6.4) und in CI erzeugt, Stichproben angesehen. Bestehender --selftest läuft weiter.
   - Nicht getestet: der eingebaute Updater von 1.9.2 auf 1.9.3 auf einem echten Windows-PC.

5. Offene Punkte / was der Nutzer tun muss
   - PR erqf1/clipline#1 (1.9.2) und diesen Branch nach main bringen, sonst bauen spätere Sessions auf 1.9.1 auf.
     Diesen Branch am einfachsten nach #1 mergen oder einen PR von claude/happy-albattani-k8ril0 nach main öffnen.
   - Einmal auf dem eigenen PC: Clipline-Update über den eingebauten Updater, prüfen, dass das Desktop-Symbol bleibt.
   - Hinweis: Wer noch 1.9.2 oder älter hat, bekommt beim Update auf 1.9.3 kein neues Symbol mehr. Ein vorhandenes Symbol
     bleibt, ein schon gelöschtes kommt nicht zurück.
   - Bilder sind auf Deutsch. Englisch geht mit: CLIPLINE_LANG=en tools/siteshots.sh build/clipline <ordner>

6. Nächste Schritte für die Haupt-Session
   - website-updates/img/*.webp nach Websites/clip übernehmen (Übersicht in website-updates/README.md;
     Namen <stil>-<modus>-<was>.webp, z. B. standard-dark-clips, standard-light-mic-test, acryl-dark-settings-look).
   - website-updates/changelog-1.9.3.html oben in Websites/clip/changelog.html einfügen (Markup an die Seite anpassen).
   - Dieselbe Desktop-Symbol-Lösung (Check: IsFirstInstall + Installer-Test) in die anderen Programme der Line-Serie übernehmen.

---------------------------------------------------------------------------
NACHTRAG 1.9.4 (Bug-Meldungen: Einstellungen-Absturz, Windows-Aufnahme startet nicht, Ruckeln unter Debian)

1. Aufgabe und Status
   - Absturz beim Speichern der Einstellungen: gefunden, behoben, mit Test abgesichert.
   - Windows-Aufnahme startet nicht: Ursache auf dem Laptop nicht sicher bekannt (kein Log). Ausweichweg eingebaut:
     nach zwei Fehlstarts GDI statt Desktop Duplication, danach libx264 statt Hardware-Encoder.
   - Ruckeln (ein Bild alle paar Sekunden) bei Debian: NICHT nachstellbar (X11 und GNOME-Wayland, PipeWire und
     PulseAudio, ffmpeg 5.1 und 6.1, Debian-12-chroot mit mutter 43 – überall flüssig). Jetzt: echte Bildrate im Log,
     bei viel zu wenig ein Hinweis an den Nutzer. Für den echten Fix wird das Log gebraucht.
2. Commits: 1a3452e (Fixes), 5ba2a41 (versehentlich eingechecktes build-dbg/ wieder entfernt – liegt aber in der
   Branch-History, beim Mergen nach main deshalb SQUASH verwenden), danach Changelog/Bericht.
   Release: v1.9.4 per workflow_dispatch. CI vorab: Release-Testlauf (ohne Version) und Linux test grün.
3. Ursache Absturz: Controller::syncRecorders() rief micMuted() auf, nachdem die Aufnahmen per std::move aus recs_
   genommen waren -> Nullzeiger bei JEDEM Speichern (seit dem Umbau der Clip-Tasten-Karten). Außerdem verschwand die
   Clip-Liste nach dem Speichern (WA_DeleteOnClose-Fenster wurde wieder gezeigt).
4. Tests: Absturz mit xdotool unter X11 nachgestellt (exit 139) und nach dem Fix 3x gespeichert ohne Absturz;
   Selbsttest speichert Einstellungen zweimal (Gegenprobe: alter Code stürzt im Test ab). Linux-CI (offscreen) und
   Windows-CI (mit echter 40-s-Aufnahme auf dem Runner) prüfen das. X11-Test prüft jetzt die Bildrate des Clips.
   Encoder-Ausweichweg lokal getestet (vorgetäuschter h264_nvenc -> nach 0,7 s libx264, Clip ok). GDI-Ausweichweg nur
   gebaut, nicht auf einem Laptop mit zwei GPUs getestet.
5. Offen / Nutzer:
   - Freund (Debian): Clipline 1.9.4 installieren, 1 Minute aufnehmen, dann ~/.local/share/WSoftware/Clipline/clipline.log
     schicken und sagen: Desktop (GNOME/KDE/…), Wayland oder X11 (echo $XDG_SESSION_TYPE), Grafikkarte.
   - Eigener Windows-Laptop: auf 1.9.4 updaten; geht die Aufnahme noch immer nicht:
     %LOCALAPPDATA%\WSoftware\Clipline\clipline.log schicken.
6. Haupt-Session: website-updates/changelog-1.9.4.html oben in Websites/clip/changelog.html einfügen.

---------------------------------------------------------------------------
NACHTRAG 1.9.5 (Einstellungen stürzten unter 1.9.4 weiter ab, Linux-Clips weiter "buggy")

- Release v1.9.5: https://github.com/erqf1/clipline/releases/tag/v1.9.5 – alle 7 Pakete, Release-Workflow und
  Linux test grün (inkl. neuer Stresstests). Commit 43387b2.
- Zweiter Einstellungen-Absturz gefunden (AddressSanitizer + neuer Stresstest clipline --settingstest):
  syncRecorders() startete Aufnahmen neu, während sie per std::move aus recs_ genommen waren -> stateChanged ->
  refreshTray() -> recording() -> Nullzeiger. Trat auf, sobald sich beim Speichern die Aufnahme ändern musste
  (Auflösung, Bildrate, Qualität, Bildschirm, Ton). Umgebaut: recs_ hat nie leere Einträge.
  Lokal 200 Runden zufällige Einstellungen (1 und 2 Bildschirme) ohne Fehler; 1.9.4 stürzt im selben Test in Runde 5 ab.
  CI: Linux (tests/settings.sh, ASan, 2 Bildschirme), Windows (Release-Workflow, echte Aufnahme, 20 Runden).
- Linux/Wayland: videorate entfernt, feste Bildrate macht ffmpeg (fps-Filter nach Wanduhr). Grund: pipewiresrc 0.3.x
  (Debian 12) stempelt echte Bilder und Keepalive-Wiederholungen mit verschiedenen Uhren; videorate könnte dann echte
  Bilder verwerfen. NICHT bewiesen (lokal ließ sich der Fehler nicht auslösen) – wenn der Freund weiter Probleme hat:
  clipline.log + kurzer Clip + echo $XDG_SESSION_TYPE $XDG_CURRENT_DESKTOP + Grafikkarte.
- Haupt-Session: website-updates/changelog-1.9.5.html oben in Websites/clip/changelog.html einfügen.
