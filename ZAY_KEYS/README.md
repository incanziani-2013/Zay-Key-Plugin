# ZAY KEYS v1.0.1 - GOLDMINE AUDIO

Instrumento virtual VST3 (Windows 64 bits) con motor SoundFont 2, teclado de 88 teclas y
interfaz inspirada en un workstation analogico. JUCE 8.0.6 + C++17 + CMake.

## Compilar
- Local: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release`
- GitHub Actions: `.github/workflows/build.yml` compila y publica `ZAY_KEYS_v1.0.1_Windows_VST3.zip`.

## Estructura
`Source/`: PluginProcessor, PluginEditor, SoundFontEngine (TinySoundFont), PresetManager,
CustomLookAndFeel, HardwareControls (botones, rueda, 7 segmentos, medidor), KeyboardComponent,
LCDDisplay, EffectsProcessor, Parameters.

## Funciones de los controles
- LCD: paginas PRESET / INSTR / PARAM / INFO, LOAD (abrir .sf2), SAVE (guardar preset).
  Botones izquierdos = elegir fila; derechos = subir, bajar, ENTER, pagina -, pagina +.
  Rueda de datos = desplazarse (o cambiar el valor en PARAM).
- PRESET SELECT: PRESET anterior/siguiente, SF2 (abrir), BANK arriba/abajo (display de 7 segmentos),
  INSTR anterior/siguiente, SAVE, RECALL.
- Polyphony 16/32/64/128 (cuenta voces/capas, ver abajo), TUNE -/+ (5 cents), PANIC.
- Glide (perilla + SWITCH), TRANSPOSE y OCTAVE (-/+).
- Sliders izquierdos: REVERB, CHORUS, DELAY (nivel de envio de cada efecto) y VOLUME.
- Filtro (Cutoff/Resonance) y tipo de filtro (pagina PARAM del LCD): filtro global post-sampler.
- Envelope ADSR: Attack/Release suman tiempo; Decay da el tiempo para llegar al nivel Sustain.

## Limitaciones conocidas (honestas)
- Bloque STYLE/SEQUENCER de la imagen de referencia: se dibuja pero NO es interactivo
  (un instrumento de muestras no tiene motor de estilos/secuenciador).
- El SoundFont de fabrica tiene 1 preset (B3 Leslie, teclas 24-101); fuera de rango se pliega por octavas.
  ZAY PIANO ORGAN usa las mismas muestras con otros ajustes hasta contar con un SF2 de piano.
- Polyphony cuenta notas x capas del instrumento (el B3 usa 2 capas L/R por nota).
- Estado "no verificado": este codigo no fue compilado ni cargado en FL Studio todavia.

## Licencias
- TinySoundFont: MIT (`third_party/tsf.h`).
- JUCE 8: AGPLv3 o licencia comercial - para distribuir cerrado se necesita licencia JUCE.
- `soundfonts/ZayGrand.sf2`: (c) 1997 Sonido Media - confirmar derechos de distribucion.
