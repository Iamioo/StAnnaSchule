# St. Anna Schule – Herr Thüner kommt

Ein Pixel-Horrorspiel in C. Du wachst im Chemieraum der St. Anna Schule auf (4. Etage),
alle sind weg – fast alle. Herr Thüner (Glatze, Laborkittel, rote Augen im Dunkeln)
ist noch da. Der Hausmeister hat alle Treppengitter abgeschlossen: Du musst dich
Etage für Etage freischalten und am Ende durch den Haupteingang entkommen.

Du bist übrigens 8 Meter groß. Deshalb ragst du über alle Wände, musst dich durch jede
Tür bücken (dann bist du langsamer) und faltest dich in Spinde zusammen.

Nur für den Privatgebrauch.

**Im Browser spielen:** https://iamioo.github.io/StAnnaSchule/ (mit Tastatur, Gamepad oder Handysteuerung).

## Steuerung

| | Tastatur | Gamepad (Switch) | Handy |
|---|---|---|---|
| Laufen | Pfeiltasten / WASD | Stick / Steuerkreuz | Steuerkreuz links |
| Rennen (kostet Puste, ist laut) | Shift | B / ZR / R | RENNEN |
| Aktion: verstecken, reden, Klavier, Sicherungen, auf-/absteigen | E / Leertaste | A | AKTION |
| Handy: Conjoke anrufen / rangehen | Q oder F | Y / X | HANDY |
| Start / weiter | Enter | + | Bild antippen |
| Pause mit Karte, Speichern, Laden | Esc / M / P | – | MENÜ / Zurück-Taste |
| Screenshot (Windows) | F12 | – | – |

## Die Schule

| Etage | Was dort ist |
|---|---|
| 6 | Haupteingang, Sekretariat, Hausmeister – hier ist der Ausgang. 7.–9. Etage: gesperrt |
| 5 | Umgang Forum (Ring aus rotem Pflaster um den Luftraum) und der Schulhof mit Dachfläche E4 – manchmal stehen hier E-Scooter und Fahrräder |
| 4 | Chemie und Musik: 4.36 Chemie-Hörsaal (hier fängst du an), 4.34/4.35 Chemie-Sammlung, 4.33 Chemie-Übung, 4.32 Musik-Vorbereitung, 4.25 und 4.26 Musik – hinter einer Glastür der **Bio-Flur** mit Biologie und Biologie-Sammlung |
| 3 | Kunst und Physik: 3.32 und 3.26 Umkleide, 3.23 und 3.20 Kunstraum, 3.14 Kunst-Vorbereitung, 3.14 A Kunst/Werkraum, 3.13 A, 3.14 B – hinter einer Glastür der **Physik-Flur** mit 3.13 und 3.09 Physik-Raum, Physik-Sammlung, 3.12 Physik-Vorbereitung, 3.04 Bücher-Keller und 3.02. Frau Niehaus ist hier |
| 2 | Sporthalle |
| 1 | Schwimmbad (die Schwimmhalle ist abgeschlossen) |

Die Etagen 3, 4 und 5 sind nach den echten Flucht- und Rettungsplänen (Ebene 3, 4, 5)
gebaut: Der orangefarbene Flur läuft um den **Luftraum Forum** herum – durch die
Glasscheiben schaust du hinunter auf die Sitzreihen des Forums. Die Pläne sind dafür
oben und unten gespiegelt, damit das Treppenhaus wie auf jeder Etage unten links liegt.

Das Treppenhaus ist immer unten links. **Hoch** geht es an der linken Wand, **runter**
an der rechten. Man kommt in der Mitte an und läuft immer von der Treppe weg – so
läuft man nicht mehr aus Versehen mehrere Etagen hintereinander. Gesperrte Treppen
haben ein Gitter mit Vorhängeschloss.

## Die Aufgaben

Jede erledigte Aufgabe öffnet den Weg weiter:

1. **4. Etage:** Taschenlampe schnappen (liegt neben dir) und den **Treppenschlüssel** finden → Gitter zur 3. Etage geht auf
2. **3. Etage:** Herrn Lerschs **4 Notenblätter** einsammeln
3. **4. Etage:** das Lied **am Klavier spielen** (Musikraum 4.25 oder 4.26) – ein Geheimfach mit dem Hallenschlüssel springt auf (das hört Herr Thüner!) → 2. Etage
4. **2. Etage:** **3 Sicherungen** suchen
5. **2. Etage:** Sicherungen in den **Sicherungskasten** im Flur → das Licht geht an, Gitter zur 1. Etage öffnet sich
6. **1. Etage:** den **Bademeister-Schlüssel** finden
7. **1. Etage:** die Schwimmhalle aufschließen und am Becken den **Schulhof-Schlüssel** holen (Conjoke schwimmt da) → 5. Etage
8. **5. Etage:** **3 Code-Zettel** auf dem Schulhof finden → das Zahlenschloss zur 6. Etage geht auf
9. **6. Etage:** den **Hausmeisterschlüssel** finden
10. **Raus durch den Haupteingang!**

Oben links steht immer das Ziel, darunter, wohin du gehen sollst. Alles liegt in jeder
Runde woanders.

## Die Leute

- **Herr Thüner** jagt dich. Seine Glatze glänzt im Dunkeln, wenn er dich jagt, leuchten
  seine Augen rot und dein Herz schlägt. Er hört dich rennen und kommt schnell die Treppe
  hinterher, auch von anderen Etagen.
- **Herr Kirschbaum** (graue Haare, Brille, Karohemd, Uhr) will dein Handy. Erwischt er
  dich damit, musst du deinen Namen aufschreiben und bei ihm warten – und das Handy ist
  für den Rest der Runde weg. Klingelt dein Handy, hört er das und kommt angerannt:
  verstecken oder schnell rangehen!
- **Herr Lersch** (Anfang 30, kurze hellbraune Haare, eckige Brille, dunkelblaues Hemd,
  Tablet in der Hand, redet mit den Händen) wartet immer auf der Etage deiner aktuellen
  Aufgabe und verrät dir, in welchem Raum das nächste Teil liegt. Wenn er redet, siehst
  du sein Porträt.
- **Frau Niehaus** (hellbrauner Dutt, lange Haare über einer Schulter, goldene Brille,
  goldene Creolen, salbeigrüner Rollkragenpullover) gibt es nur in der **3. Etage**. Sie
  geht von Klassenraum zu Klassenraum und summt – ihre Noten siehst du auch im Dunkeln.
  Bist du mit ihr **im selben Klassenraum** und gehst nicht schnell weg, **musst du mit ihr
  singen**: ein paar Sekunden kannst du dich nicht bewegen, und **Herr Thüner wird
  alarmiert** (auch von einer anderen Etage). Hast du Herrn Lerschs Notenblätter dabei,
  will sie doppelt so schnell singen. Im Flur oder im Spind bist du sicher.
- **Die Putzkräfte** (blaue Uniform, Mütze, Wischmopp) wischen die Flure und ziehen von
  Etage zu Etage. Wo sie gewischt haben, sind blaue Pfützen (sie glitzern auch im
  Dunkeln) und steht ein gelbes Warnschild. **Rennst** du über nassen Boden oder direkt an ihnen vorbei, **rutschst du
  aus** (das ist laut). Gehen ist kein Problem. Sehen sie dich zu lange („?“, dann „!“),
  **rufen sie Herrn Thüner**. Aber: Auch Herr Thüner und Herr Kirschbaum rutschen aus,
  wenn sie hinter dir her über den nassen Boden rennen!
- **Conjoke** ist groß und stark. Ruf ihn an (oder geh ran, wenn er anruft), dann kommt er
  und prügelt sich mit einem Lehrer – der ist danach eine Weile langsamer.

## Items und Sammelsachen

| Item | Wirkung |
|---|---|
| Taschenlampe | viel mehr Licht (ohne sie hast du nur das Handylicht) |
| Turnschuhe (2. Etage) | du läufst und rennst schneller |
| Energy-Drink | wird automatisch getrunken, wenn dir die Puste ausgeht: 8 Sekunden Vollgas |
| Stinkbombe | rettet dich einmal, wenn Herr Thüner dich packt |
| 18 St.-Anna-Sticker | 3 pro Etage, gut versteckt (sie glitzern nicht) – zum Sammeln |

**E-Scooter und Fahrräder (5. Etage):** Manchmal stehen auf dem Schulhof welche.
Mit der Aktionstaste steigst du auf und bist richtig schnell – aber nur ein paar
Sekunden: Beim E-Scooter ist der Akku leer, beim Fahrrad reißt die Kette. Dann ist er
kaputt. Oben siehst du, wie viel Akku oder Kette noch übrig ist.

## Schwierigkeit

Im Titelbild wählst du mit den Pfeilen (links/rechts) den Modus:

| Modus | Checkpoints | Speichern | Putzkräfte | Frau Niehaus |
|---|---|---|---|---|
| Einfach | ja | ja | erkennen dich nach 7 Sekunden, sehen 6 Kacheln weit | lässt dir viel Zeit |
| Mittel | ja | ja | nach 5 Sekunden, 8 Kacheln weit | normal |
| Schwer | **nein** – erwischt heißt alles von vorn | nein | nach 4 Sekunden, 9 Kacheln weit | schnell |

Auf Einfach ist Herr Thüner außerdem ein bisschen langsamer.

## Pause, Karte, Speichern

Mit **Esc** (oder M / P, am Handy MENÜ) hält das Spiel an. Du siehst die **Karte der
Etage** – aber nur die Teile, die du schon gesehen hast. Rot blinkend bist du, blau
Herr Lersch, gelb blinkend das Teil, das er dir verraten hat. Darunter kannst du
**speichern**, einen **Spielstand laden** oder zum Hauptmenü. Im Browser bleibt der
Spielstand auch nach dem Schließen erhalten; im Titelbild steht dann
„Spielstand laden“.

**Checkpoints:** Wenn Herr Thüner dich erwischt, geht es im Treppenhaus der aktuellen
Etage weiter. Alles, was du aufgesammelt hast, behältst du. Am Ende siehst du deine Zeit,
deine Sticker und wie oft du erwischt wurdest.

## Die Versionen

| Plattform | Datei | Status |
|---|---|---|
| Browser (PC und Handy) | `web/` + `src/plat_web.c` | wird von GitHub Actions gebaut und auf GitHub Pages veröffentlicht |
| Windows 95 bis 11 | `bin/StAnna.exe` | gebaut und getestet, mit Musik – **noch die alte Version** (mit `build_windows.bat` neu bauen) |
| Android 4.1+ (ARM64 + ARMv7) | `bin/StAnna.apk` | gebaut und im Emulator getestet, Ton ab Android 8 – **noch die alte Version** |
| Nintendo Switch (Homebrew) | `bin/StAnna.nro` | gebaut, startet in Ryujinx – **noch die alte Version** |
| Linux / Unix (X11) | `build_linux.sh` | Quelltext fertig und syntaxgeprüft, hier **nicht gelinkt** (kein Linux auf diesem PC) |
| TempleOS | `templeos/StAnna.HC` | HolyC-Quelltext generiert, **ungetestet** |

### Browser
Die Webseite (`web/index.html`, `web/stanna.js`) lädt `stanna.wasm`, das GitHub Actions
bei jedem Push mit clang aus `src/game.c` und `src/plat_web.c` baut – ohne Emscripten
und ohne C-Bibliothek (`.github/workflows/pages.yml`). Die Handysteuerung lässt sich
ein- und ausschalten, die Bestzeit wird im Browser gespeichert und kann geteilt werden.

### Windows
`bin/StAnna.exe` doppelklicken. Die exe nutzt keine C-Laufzeitbibliothek und nur
Windows-Funktionen, die es schon in Windows 95 gab (GDI, winmm). Musik und Effekte
kommen aus einem eigenen kleinen Chiptune-Synthesizer im Spielkern.

Selbst bauen (Visual Studio 2017+): `build_windows.bat`

### Android
`bin/StAnna.apk` aufs Handy kopieren und installieren („Unbekannte Apps zulassen“).
Läuft im Querformat. Selbst bauen: `android/build_android.ps1` – braucht **kein**
Android NDK (devkitPro-GCC + `ld.lld` aus den SDK-Build-Tools + Stub-Bibliotheken).

### Nintendo Switch
`bin/StAnna.nro` nach `/switch/` auf die SD-Karte kopieren und über das Homebrew-Menü
starten. Selbst bauen: `switch/build_switch.ps1` (devkitPro mit devkitA64 + libnx).
Eine NSP (Home-Menü-Symbol) braucht zusätzlich das Tool hacbrewpack und die eigenen
Konsolen-Schlüssel (prod.keys) – die sind hier nicht dabei.

### Linux
```
sh build_linux.sh
./bin/stanna
```
Braucht `gcc` und die X11-Header (`sudo apt install gcc libx11-dev`). Läuft mit jedem
X-Server (8/16/24/32 Bit Farbtiefe, auch XWayland), Joystick über `/dev/input/js0`.
Ton über `aplay` (Paket alsa-utils), sonst bleibt es still.

### TempleOS
`templeos/StAnna.HC` in TempleOS kopieren (z. B. über ein Disk-Image) und im
Terminal `#include "StAnna.HC";` eingeben. Musik und Effekte über den PC-Lautsprecher.
Die Datei wird von `tools/make_holyc.py` aus `src/game.c` und `src/game_data.inc`
erzeugt. Der Spielkern ist absichtlich in einem C-Stil geschrieben, den HolyC versteht
(keine structs, kein `?:`, Bitoperationen immer geklammert). Weil es hier kein TempleOS
zum Ausprobieren gab, kann es sein, dass noch Kleinigkeiten angepasst werden müssen.

## Technik

- 320×200 Pixel, 16 Farben, alles Pixel-Art – komplett selbst gezeichnet im Code.
  Keine fremden Grafiken, keine Fotos.
- 6 Etagen à 72×28 Kacheln, Taschenlampe mit 4 Helligkeitsstufen und Bayer-Dithering,
  Sichtlinien pro Kachel, Lampen und Mondlicht.
- Lehrer suchen dich per Breitensuche (BFS), wechseln abstrakt die Etagen und folgen dir
  über die Treppe.
- Feste 30 Ticks pro Sekunde, nur Ganzzahl-Rechnung – läuft auch auf uralten Rechnern.
- `tools/progress_test.c` spielt mit 20 Seeds alle Aufgaben durch (Erreichbarkeit,
  Gitter, Treppen, Checkpoint, Ausgang) – `tools/build_test.bat`.
- `tools/sim.c` lässt einen Bot ganze Runden gegen die Lehrer spielen – `tools/build_sim.bat`.
- `StAnna.exe -shot N bild.bmp` rendert Testszenen, `-world F bild.bmp` eine ganze Etage
  (so sind die Bilder in `shots/` entstanden).

## Dateien

```
src/game.c           Spielkern (Logik, Grafik, KI, Musik) – für alle Plattformen
src/game_data.inc    Daten: Pixel-Art, Etagen, Räume, Aufgaben, Items, Musik
src/game.h           Schnittstelle Kern <-> Plattform
src/plat_win32.c     Windows
src/plat_android.c   Android (NativeActivity, ohne Java)
src/plat_switch.c    Nintendo Switch (libnx)
src/plat_x11.c       Linux/Unix
src/plat_web.c       Browser (WebAssembly), dazu web/
templeos/StAnna.HC   TempleOS (generiert), templeos/platform.HC = TempleOS-Teil
android/, switch/    Build-Skripte
tools/               Prüf- und Hilfsskripte, Tests, Balance-Simulation
shots/               Screenshots
```
