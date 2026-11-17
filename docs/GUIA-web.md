# Guía de la web LTC FilesGen

Web: https://oscavlab.github.io/LTC_FilesGen/

## Pestaña «Generar LTC» (`index.html`)
Genera archivos WAV de LTC para poner en una pista de audio.

- **Offset inicial**: hora del primer frame (hh:mm:ss:ff).
- **Duración por archivo**.
- **FPS**: 23.976, 24, 25, 29.97 o 30. Con 29.97 se puede elegir **Drop Frame** o **Non-Drop**.
- **Multi-Export**: varios archivos a la vez, cada uno con su offset separado por el **intervalo** (hh:mm). Se descargan juntos en un ZIP.
- **Bit depth** (16/24) y **sample rate** (44.1 / 48 / 96 kHz).
- Nombre del archivo: `LTC_Off <offset> _ Dur <duración> _ <fps> <NDF|DF>fps | oscLAB.wav`

## Pestaña «LTC → MTC» (`mtc.html`)
Convierte LTC en MTC regenerado (como una MOTU) por Web MIDI. **Funciona en Chrome o Edge**.

1. **Fuente**:
   - *LTC por audio*: elige la entrada y el canal, y pulsa **Escuchar**. El vúmetro debe moverse.
   - *Generador interno*: hora de inicio, frame rate, Play / Pausa / Stop.
2. **Salida MTC**: puerto MIDI (en Mac, el *IAC Driver* para mandarlo a Live en el mismo ordenador), rate MTC (Auto = el del LTC), freewheel y compensación en ms.
3. Arriba se ve el timecode, el estado (BUSCANDO LTC, LOCK, FREEWHEEL, SIN SEÑAL, RELOCATE), el rate, el jitter y la velocidad.
4. Configura Live como se explica en [LTC-MTC-explicacion.md](LTC-MTC-explicacion.md#3-ableton-live-como-esclavo-de-mtc).

Consejos:
- Entrada de audio limpia, sin procesado. La web ya desactiva la cancelación de eco, la supresión de ruido y el control automático de ganancia.
- Si Live va un poco adelantado o atrasado, ajusta **Compensación** (web) o **Sync Delay** (Live).

Para directo, mejor la **app Mac** ([app/README.md](../app/README.md)): usa el reloj de la tarjeta, CoreMIDI y un puerto virtual propio.
