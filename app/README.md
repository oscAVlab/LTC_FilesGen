# osc[AV]lab LTC (app para Mac)

Lee **LTC (SMPTE por audio)** desde cualquier entrada de tu interfaz, o lo genera, y saca **MTC regenerado** que Ableton Live sigue bien. Funciona como el conversor LTC→MTC de una interfaz MOTU:

- Decodifica el LTC con precisión de muestra (biphase mark) y detecta solo el rate: 23.976, 24, 25, 29.97 DF/ND y 30.
- Engancha un reloj propio al LTC con un PLL (fase + velocidad, admite varispeed) y tiene **freewheel** si la señal se corta.
- Emite los **Quarter Frames** perfectamente espaciados, con marca de tiempo de CoreMIDI tomada del reloj de la tarjeta de audio, y manda un **Full Frame** al enganchar o al saltar.
- Crea un puerto MIDI virtual **«osc[AV]lab MTC»**, así que no hace falta el IAC Driver. También puede salir por cualquier interfaz MIDI.
- **Generador interno**: MTC desde una hora de inicio (Play, Pausa y Stop), y opcionalmente LTC por una salida de audio.
- **Generar archivos**: los mismos WAV de LTC que la web (offset, duración, fps, DF/NDF, multi-export).

## Instalar / compilar

Requisitos (una vez): `xcode-select --install` y `brew install cmake`

```zsh
git clone https://github.com/oscAVlab/LTC_FilesGen.git "$HOME/_LAB/Devices/LTC"
zsh "$HOME/_LAB/Devices/LTC/app/mac/Compilar.command"
```

Para actualizar: `zsh "$HOME/_LAB/Devices/LTC/app/mac/Actualizar.command"`

## Ableton Live

1. En *Settings › Link, Tempo & MIDI*, elige el puerto «osc[AV]lab MTC» y pon **MIDI Sync Type = MIDI Timecode**.
2. En **MIDI Timecode Rate**, el rate que muestra la app (o *SMPTE All*).
3. En **MIDI Timecode Offset**, la hora del LTC que corresponde al compás 1.
4. Activa **EXT** en la barra de control.

## Pruebas del motor

```zsh
cmake -B build-tests -DLTC_APP=OFF -DLTC_TESTS=ON && cmake --build build-tests --target LtcTests && ./build-tests/LtcTests_artefacts/Release/LtcTests
```

Las pruebas cubren: ida y vuelta LTC en todas las rates, cruce de medianoche y de drop frame, intervalo y fase de los Quarter Frames, varispeed ±3 %, cortes de señal (freewheel / parada) y la salida LTC del generador.
