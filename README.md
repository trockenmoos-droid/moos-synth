# MOOS – Wavetable Pad Synth (VST3)

Polyphoner Wavetable-Synth für Flächen und Ambient-Pads. C++ / JUCE 8.

## Klangerzeugung
- **2 Wavetable-Oszillatoren** – 8 Frames (Sine, Triangle, Saw, Square, Voice, Organ, Glass, Dust), stufenlos morphbar, bandbegrenzt (kein Aliasing)
- **Unison** – bis 7 Stimmen pro Oszillator, Detune + Stereobreite, zufällige Startphasen
- **Filter** – 12-dB-Tiefpass (State Variable), Resonanz, eigene ADSR-Hüllkurve (±)
- **LFO** – moduliert Wavetable-Position und Cutoff
- **FX** – Chorus, Reverb, Soft-Clip am Ausgang
- 16 Stimmen, Pitchbend ±2 Halbtöne, Velocity → Lautstärke

## Windows-Build (ohne eigenen Compiler)
1. Dieses Projekt in ein GitHub-Repository pushen.
2. GitHub baut automatisch (Tab **Actions** → letzter Lauf, ca. 10 min).
3. Artefakt **MOOS-VST3-Windows** herunterladen, entpacken.
4. Den Ordner `MOOS.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
5. In der DAW Plugins neu scannen.

Zusätzlich gibt es `MOOS.exe` als Standalone-Version (zum Testen ohne DAW).

## Lokal bauen
```
cmake -B build
cmake --build build --config Release
```
Offline-Test (rendert einen Akkord nach WAV): `-DMOOS_BUILD_RENDER_TEST=ON`, dann `MoosRender out.wav`.

## Struktur
- `Source/Wavetable.h` – Erzeugung der Wellenformen (per inverser FFT, 11 Mipmap-Stufen)
- `Source/SynthVoice.h` – eine Stimme: Oszillatoren, Unison, Filter, Hüllkurven, LFO
- `Source/PluginProcessor.*` – Parameter, Stimmenverwaltung, Effekte, Preset-Speicherung
- `Source/PluginEditor.*` – Oberfläche
