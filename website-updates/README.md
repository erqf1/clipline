# Website-Updates für mert.win/clip

Die Website liegt nicht in diesem Repository. Hier liegt, was in `Websites/clip` übernommen werden soll.

- `changelog-1.9.3.html`: Eintrag für `changelog.html`, oben über 1.9.2 einfügen.
- `img/`: echte Bilder der Fenster als WebP, unsichtbar erzeugt mit `tools/siteshots.sh` (Deutsch, zwei Bildschirme).

## Bilder

Namen: `<stil>-<modus>-<was>.webp`, Stil `standard`, `minimal` oder `acryl`, Modus `dark` oder `light`.

| Was | Datei |
| --- | --- |
| Clip-Liste mit Vorschaubildern | `*-clips` |
| Einstellungen: Allgemein, Ton, Clip-Tasten, Aussehen | `*-settings-general`, `-sound`, `-keys`, `-look` |
| Bildschirmwahl aufgeklappt (zwei Bildschirme) | `standard-*-screen-select` |
| Taste wird aufgenommen („Tasten drücken…“) | `standard-*-key-record` |
| Mikrofon: Pegel grün, Test läuft („Sprich jetzt… 3“) | `standard-*-mic-test` |
| Ersteinrichtung, Schritt 1 bis 4 | `standard-*-setup-1` … `-4` |
| Andere Akzentfarben | `accent-violet-acryl-dark-*`, `accent-blue-standard-light-*`, `accent-cyan-minimal-dark-*` |

Einstellungen 780×760, Clip-Liste 1060×700, Einrichtung 920×680 Pixel.

Neu erzeugen (Linux, braucht ffmpeg und cwebp):

```sh
cmake -S . -B build -G Ninja && cmake --build build
tools/siteshots.sh build/clipline website-updates/img          # Englisch: CLIPLINE_LANG=en tools/siteshots.sh …
```
