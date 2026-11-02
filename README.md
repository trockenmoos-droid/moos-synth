# MOOS – Wavetable Pad Synth (VST3)

Polyphoner Wavetable-Synth für Flächen, Drones und Ambient. C++ / JUCE 8. Von trockenmoos.

## Klangerzeugung
- **2 Wavetable-Oszillatoren** – 8 Werks-Frames (Sine, Triangle, Saw, Square, Voice, Organ, Glass, Dust), stufenlos morphbar, bandbegrenzt
- **Eigene Wavetables** – Knopf *WAV* lädt WAV/AIFF/FLAC:
  - Länge = Vielfaches von 2048 Samples (bis 64 Frames) → klassische Wavetable (Serum-/Vital-Format)
  - jede andere Datei (z. B. Field Recording) → 16 Ausschnitte, gleichmäßig über die Datei verteilt
- **Unison** bis 7 Stimmen, Detune, Stereobreite, **Analog Drift**
- **Filter** – Low Cut (12 dB HP) + 12-dB-Tiefpass mit Resonanz und eigener Hüllkurve
- **Spielweise** – 16 Stimmen oder **Mono/Legato**, **Glide**, Pitchbend-Bereich, Sustain-Pedal, Velocity

## Modulation
- **LFO 1** (Sine/Triangle/Saw/Square/S&H) mit festen Zielen Position und Cutoff
- **LFO 2** (frei zuweisbar)
- **Mod-Matrix** mit 4 Slots
  - Quellen: LFO 1, LFO 2, Mod Wheel, Aftertouch (Kanal + polyphon), Velocity, Filter-Hüllkurve, Zufall pro Note, Key Track
  - Ziele: Position A/B, Cutoff, Resonanz, Pitch, Fine B, Level A/B, Unison Detune, Lautstärke

## Effekte & Arpeggiator
- Chorus, **Ping-Pong-Delay** (frei oder taktsynchron), **Hall mit Shimmer** (Oktave nach oben)
- **Arpeggiator** – Up, Down, Up/Down, Random, As Played; 1/4 bis 1/32 inkl. Triolen; 1–4 Oktaven; Gate. Rastet auf das Taktraster der DAW ein, wenn sie läuft.

## Presets
10 Werkspresets (auch als DAW-Programme). Eigene Presets über *SPEICHERN* →
`Dokumente\trockenmoos\MOOS Presets\*.moospreset`. Geladene Wavetables werden mitgespeichert (als Dateipfad).

## Oberfläche
3 Seiten (Klang / Modulation / FX & Arp), frei skalierbar. Hintergrund: prozedural erzeugte Moos-Textur
oder ein eigenes Foto (*HINTERGRUND* → *Eigenes Foto laden*); gilt für alle MOOS-Instanzen.

## Windows-Build
Jeder Push baut automatisch (GitHub → **Actions**). Artefakt **MOOS-VST3-Windows** laden,
Ordner `MOOS.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren, DAW neu scannen.

## Lokal bauen
```
cmake -B build
cmake --build build --config Release
```
Offline-Tests (Presets, Wavetable-Import, Preset-Speichern, Shimmer-Stabilität, Screenshots, Hörprobe):
`-DMOOS_BUILD_RENDER_TEST=ON`, dann `MoosRender out.wav`.

## Struktur
- `Source/Wavetable.h` – Wavetables (Werks-Frames, WAV-Import, 11 Mipmap-Stufen per FFT)
- `Source/SynthEngine.h` – Stimmenverwaltung, Oszillatoren, Filter, Hüllkurven, LFOs, Mod-Matrix, Drift, Glide
- `Source/Arpeggiator.h`, `Source/Effects.h` – Arp, Low Cut, Delay, Shimmer-Hall
- `Source/Params.h`, `Source/Presets.h` – Parameter und Werkspresets
- `Source/MossBackground.h` – Moos-Textur / eigenes Foto
- `Source/PluginProcessor.*`, `Source/PluginEditor.*` – Plugin und Oberfläche
