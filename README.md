# trockenmoos audio – Plugins aus dem Salzkammergut

Freie Synths und Effekte (VST3, Windows), benannt nach Orten und Geschichten aus dem Salzkammergut.
C++ / JUCE 8. Von trockenmoos.

| Plugin | Art | Kurz |
|---|---|---|
| **MOOS** | Synth | Wavetable-Pad-Synth: 2 Oszillatoren, eigene WAV-Wavetables, Mod-Matrix, Arp, Shimmer |
| **HALLSTATT** | Hall | 5 Räume: Salzbergwerk, Beinhaus, Pfarrkirche, Hallstättersee, Eishöhle · Freeze |
| **SOLELEITUNG** | Delay | Jede Wiederholung fließt durch Rohr, Salz und Strömung · bis zu 4 Stationen · Absperren |
| **DACHSTEIN** | Synth | Gletscher-Drone: additive, dehnbare Obertöne, Fels, Wind, Eisknistern, Hall |

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

## Gemeinsam
- Werkspresets + eigene Presets (*SPEICHERN* → `Dokumente\trockenmoos\<PLUGIN> Presets`)
- Hintergrund: prozedurale Landschaft je Plugin (Moos, See in der Abenddämmerung, Holzrohr mit Salzkruste, Gletschereis)
  oder eigenes Foto (*HINTERGRUND*)
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
Noch offen (geplant: frei / Spende). JUCE wird unter AGPLv3 oder kommerzieller Lizenz angeboten – vor einer
öffentlichen Veröffentlichung klären.
