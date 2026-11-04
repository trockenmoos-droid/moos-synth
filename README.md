# trockenmoos audio – Plugins aus dem Salzkammergut

Freie Synths und Effekte (VST3, Windows), benannt nach Orten und Geschichten aus dem Salzkammergut.
C++ / JUCE 8. Von trockenmoos.

| Plugin | Art | Kurz |
|---|---|---|
| **MOOS** | Synth | Wavetable-Pad-Synth: 2 Oszillatoren, eigene WAV-Wavetables, Mod-Matrix, Arp, Shimmer |
| **HALLSTATT** | Hall | 5 Räume: Salzbergwerk, Beinhaus, Pfarrkirche, Hallstättersee, Eishöhle · Freeze |
| **SOLELEITUNG** | Delay | Jede Wiederholung fließt durch Rohr, Salz und Strömung · bis zu 4 Stationen · Absperren |
| **DACHSTEIN** | Synth | Gletscher-Drone: additive, dehnbare Obertöne, Fels, Wind, Eisknistern, Hall |
| **SARSTEIN** | Verzerrer | 4 Gesteine (Kalk, Granit, Fels, Bergkristall), Geröll, Steinschlag, 4x Oversampling, Auto-Pegel |
| **5 FINGERS** | Granular-Delay | 5 Abgriffe wie die Plattform am Krippenstein, jeder mit Körnung → Granular-Wolke (Tonhöhe, Rückwärts) |
| **SANKT WOLFGANG** | Synth | Kirchenglocke (Hum, Prim, Terz, Quint, Nominal … schwebend) + Pfeifenorgel (7 Register, Chiff, Tremulant) |
| **LANDFRIEDALM** | Synth | Zither & Hackbrett (Saitenmodell), Kuhglocke, Jodler mit Registersprung; Echo vom Berg |
| **KELTEN** | Synth | Carnyx-Kriegshorn (Growl, rasselnde Zunge), Bordun (Grundton + Quinte), Eisenzeit-Lo-Fi |
| **RÖMER** | Synth | Hydraulis (Wasserorgel mit schwankendem Druck und Blubbern) + Cornu; Villa, Therme, Arena |
| **GOISERER PASCHER** | Rhythmus | Gate + Klatscher-Gruppe im 16er-Raster, Dreivierteltakt-Muster, Swing, Mensch, Schmuggel |
| **HAMMERSCHMIED** | Drum-Maschine | Generativ: 6 Spuren (Synth oder eigene Samples) mit Euklid-Rhythmen, eigene Tempi, Chaos, Mutation, Granular-„Glut“ und Stotter; folgt DAW-Start/Stopp |

## HALLSTATT
Frühe Reflexionen je Raum (Flatterecho im Stollen, Felswand-Echos über dem See …) + 8-Kanal-Feedback-Delay-Netzwerk
mit getrennter Nachhallzeit für Bass, Mitten und Höhen. Regler: Größe, Nachhall (bis 30 s), Predelay, frühe Reflexionen,
Dämpfung, Bass, Modulation, Breite, Low/High Cut, Ducking, Mix, Freeze.

## SOLELEITUNG
Benannt nach der Soleleitung Hallstatt–Ebensee (seit 1595, die älteste Pipeline der Welt).
Zeit frei oder im Songtempo, Feedback bis 110 % (Salz begrenzt weich), **Rohr** (Resonanz) + Rohrton,
**Salz** (Sättigung), **Strömung** (Wow/Flutter), Low Cut, Ton, Breite (Ping-Pong), Ducking,
**Stationen** (1–4 Abgriffe: Bad Goisern, Lauffen, Bad Ischl, Ebensee), **Absperren** (Endlosschleife).

## DACHSTEIN
Bis zu 32 Teiltöne pro Stimme, 8 Stimmen. **Helligkeit**, **Kristall** (dehnt die Obertöne zu glockigen Spektren),
**Spalten** (nimmt gerade Obertöne weg → hohl), **Bewegung/Tempo** (jeder Teilton atmet eigenständig),
**Schmelze** (Verstimmung), **Fels** (Sub-Oktave), **Wind** mit Farbe und Böen, **Knistern** (Eis), Hall.
Mod Wheel → Helligkeit, Aftertouch → Bewegung, Sustain-Pedal.

## SARSTEIN
Tiefe (Hochpass vor der Verzerrung) → Drive bis 48 dB → Kennlinie je Gestein (4-faches Oversampling) →
Steinschlag (Bit-/Ratenreduktion) → Ton (Tilt um 900 Hz) → High Cut → Auto-Pegel → Mix (phasenrichtig).
Kalk = weich, Granit = hart, Fels = röhrig/asymmetrisch, Bergkristall = Wavefolder. Geröll = Asymmetrie (gerade Obertöne).

## 5 FINGERS
Gemeinsame Länge (frei oder im Songtempo); je Finger Zeit (Anteil der Länge), Pegel, Pan, **Körnung**.
Körner: Größe (10–400 ms), Tonhöhe (±12 Halbtöne), Streuung, Dichte, Rückwärts. Feedback vom längsten Finger –
mit Tonhöhe +12 entsteht ein Shimmer-Delay.

## SANKT WOLFGANG
Glocke: 9 Teiltöne einer Kirchenglocke als schwebende Paare mit eigenen Abklingzeiten; Terz Moll/Dur, Material
(Unharmonie), Schwebung, Anschlag, Dämpfen. Orgel: 16', 8', 4', 2 2/3', 2', 1 3/5', Mixtur, Chiff, Wind, Tremulant.
Mod Wheel = Schwellwerk, Aftertouch = Tremulant. Kirchenhall.

## LANDFRIEDALM
Zither (gezupft, Zupfposition = Anschlag), Hackbrett (Doppelsaiten, Hammer), Kuhglocke (unharmonische Teiltöne,
Klappern), Jodler (Formant-Stimme a-o-u-e-i, Brust- → Kopfstimme mit Jodelsprung, Vibrato). Holzkörper, Echo vom Berg.

## KELTEN
Carnyx: Blechblas-Stimme, deren Helligkeit mit der Lautstärke wächst; Growl, Zunge (Rasseln), Anblasen (von unten in
den Ton). Bordun auf wählbarem Ton + Quinte, setzt beim Spielen ein. Eisenzeit: Erde (Rumpeln, Knistern), Rost
(Bits/Rate), Alter (Bandbreite, Leiern).

## RÖMER
Hydraulis: 1–3 Pfeifenreihen, Wasserdruck lässt Lautstärke und Stimmung schwanken, Blubbern im Kessel, Luft.
Cornu: rundes Bronzehorn. Ort: Villa, Therme, Arena.

## GOISERER PASCHER
Zwei Reihen à 16 Schritte (Gate, Klatschen) im Takt der DAW; Raster 1/8 bis 1/32, Länge 2–16 (12 = zwei
Dreivierteltakte), Swing, Schmuggel (Schläge fallen weg oder kommen dazu). Gate: Tiefe, Glätten, Mix.
Klatscher: Anzahl (1–6), Mensch (Timing-Streuung), Hände (Klangfarbe), Wirtshaus (Raum).

## HAMMERSCHMIED
Benannt nach der alten Hammerschmiede in Goisern. Sechs Spuren (Hammer, Amboss, Blasebalg, Esse, Funke, Zange), alle
synthetisch – oder mit eigenen Samples. Jede Spur: Quelle (Synth / Sample / Beide), Pegel, Ton, Abklingen, Schritte (2–32), Schläge, Drehung (Euklid-Muster), Chance und Tempo
(1/2 … 2 × 16tel – Polymetrik). Samples: ins SAMPLE-Feld ziehen oder klicken (WAV/AIFF/FLAC/OGG/MP3, max. 10 s);
ein Ordner oder mehrere Dateien füllen die Spuren der Reihe nach („Kit laden“). Ton = ±12 Halbtöne, Abklingen ganz
rechts = ganzes Sample. Samples bleiben beim Presetwechsel, werden im DAW-Projekt und in eigenen Presets (als Pfad)
gespeichert. Takt: folgt Start/Stopp der DAW, jeder Start beginnt bei Takt 1 mit unmutiertem Muster.
Werk: Frei (läuft auch ohne DAW-Transport; im Standalone immer), Swing, Chaos (Timing, Ratschen, Verstimmung),
Mutation (Muster wandern Takt für Takt), Hitze (Sättigung). Glut: Granular-Wolke aus den letzten 3 Sekunden
(Korn, Dichte, Tonhöhe, Rückwärts, Streuung) und Stotter-Wiederholungen. MIDI-Noten C1–F1 (36–41) spielen die Spuren.

## Gemeinsam
- Werkspresets + eigene Presets (*SPEICHERN* → `Dokumente\trockenmoos\<PLUGIN> Presets`)
- Hintergrund (*HINTERGRUND*): jede Landschaft der Serie in jedem Plugin (Moos, Hallstättersee am Abend, Holzrohr mit Salz,
  Gletscher, Kalkwand, Wolkenmeer, Glasfenster, Almwiese, keltische Bronze, römisches Mosaik, Hirschleder, geschmiedetes Eisen) oder ein eigenes Foto; dazu „Durchsicht der Felder“ (wenig/mittel/viel)
- Frei skalierbare Oberfläche

## Installation (Windows)
GitHub → **Actions** → neuester Lauf → **trockenmoos-plugins-VST3-Windows** laden, entpacken,
die `.vst3`-Ordner nach `C:\Program Files\Common Files\VST3\` kopieren, DAW neu scannen.

## Bauen
```
cmake -B build
cmake --build build --config Release
```
Offline-Tests (Stabilität, Presets, Hörproben, Screenshots): `-DTM_BUILD_TESTS=ON`, dann z. B. `HallstattTest <ordner>`.

## Struktur
- `common/tm/` – gemeinsame Basis: Prozessor mit Presets, Editor-Gerüst, Look & Feel, Landschafts-Texturen, FDN-Hall
- `plugins/<name>/Source/` – je Plugin Prozessor und Editor
- `tests/` – Offline-Tests

## Lizenz
Die Plugins sind und bleiben gratis (keine Bezahlversion, keine Spenden-Einnahmen).
Gebaut mit JUCE 8 unter der kostenlosen Starter-Lizenz (laut JUCE-EULA bis 20.000 USD Jahresumsatz/Förderung,
ohne Pflicht zu offenem Quellcode oder Logo) – vor Veröffentlichung in der aktuellen EULA auf juce.com prüfen.
