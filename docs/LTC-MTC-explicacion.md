# SMPTE / LTC → MTC: cómo funciona y por qué la MOTU sí engancha Ableton

## 1. LTC (SMPTE por audio)
- Es una **señal de audio** (biphase mark) que transmite **80 bits por frame**.
- Cada frame lleva la hora completa **hh:mm:ss:ff**, el flag de **drop frame** (bit 10), bits de usuario, un bit de corrección de polaridad y una **palabra de sincronía** fija: `0011111111111101` (bits 64–79).
- Codificación biphase: hay un flanco al inicio de cada bit, y un "1" tiene además otro flanco a mitad del bit. No depende de la polaridad y se puede leer a varias velocidades.
- Es continuo y preciso a nivel de muestra: **cada frame dice exactamente dónde estás**.

## 2. MTC (MIDI Time Code)
Usa dos tipos de mensaje:

| Mensaje | Bytes | Para qué |
|---|---|---|
| **Full Frame** | `F0 7F 7F 01 01 hh mm ss ff F7` (SysEx) | Saltar/posicionar. El rate va en los bits 5–6 de `hh`. |
| **Quarter Frame** | `F1 0nnn dddd` | Reloj en marcha: 4 por frame. Cada uno lleva un nibble; con 8 (2 frames) se reconstruye la hora. |

- Códigos de rate: `0` = 24, `1` = 25, `2` = 29.97 drop, `3` = 30. **29.97 non-drop no tiene código** y viaja como "30". 23.976 viaja como "24".
- La hora de un ciclo de 8 piezas corresponde al frame en que se envió la pieza 0. El receptor le suma 2 frames.
- **Lo importante:** el receptor (Ableton) **usa el ritmo con que llegan los Quarter Frames como su reloj**. Si llegan irregulares, Live tartamudea, salta o no engancha.

## 3. Ableton Live como esclavo de MTC
Según el manual de Live 12, *Synchronizing with Link, Tempo Follower and MIDI*:

- En *Settings › Link, Tempo & MIDI*, en la lista de puertos MIDI, elige el puerto y pon **MIDI Sync Type = MIDI Timecode**.
- **MIDI Timecode Rate**: el rate de la fuente (o *SMPTE All* para autodetección).
- **MIDI Timecode Offset**: la hora SMPTE que Live trata como inicio del Arrangement.
- Activa **EXT** en la barra de control (o *Options › External Sync*). El LED junto a EXT parpadea cuando recibe sync válido.
- El MTC no lleva tempo ni compás: el tempo lo pones tú. Con MTC, Live solo puede ser esclavo.
- **Sync Delay** (por dispositivo) compensa retrasos de transmisión.

## 4. Por qué fallaban los intentos de mandar MTC por software
Las causas más probables:

1. **Jitter**: el generador manda los Quarter Frames desde timers del sistema (o de JavaScript), que se retrasan de forma irregular.
2. **Rate distinto** entre la fuente y Live. Ojo con 29.97 ND, que viaja como "30".
3. **Offset**: si el LTC empieza en `01:00:00:00` y el offset de Live está en 0, Live salta una hora adelante.
4. **Solo se mandan Full Frames**, o el puerto no está en *MIDI Timecode* o EXT no está activado.

## 5. Por qué la MOTU 828es sí funciona
Según el manual MOTU AVB (1248/828es), *LTC-to-MTC Conversion*:

- La interfaz convierte el LTC de una entrada en MTC cuando está conectada a un Mac. Se elige el canal con *Computer Channel for LTC-to-MTC Conversion*. No hace falta que el Clock Mode esté en LTC.
- El MTC aparece en un puerto MIDI del driver ("SMPTE Sync port").
- Con **Enable Jam Sync** sigue generando MTC aunque el LTC se corte (freewheel).

La clave es que **no traduce mensaje a mensaje, sino que regenera** el MTC:
- decodifica el LTC en hardware, con precisión de muestra;
- engancha su propio reloj al LTC (un motor de *phase lock*, que en sus MIDI Timepiece se llama así);
- emite Quarter Frames perfectamente espaciados desde ese reloj.

(Que la 828es use el mismo motor de phase lock es una suposición razonable, no confirmada en su manual.)

## 6. Lo que hacen la web y la app osc[AV]lab (lo mismo que la MOTU)
1. **Decodificador LTC** con precisión de muestra. El fin de cada frame es el flanco que inicia el siguiente.
2. **Detección de rate** (23.976 / 24 / 25 / 29.97 DF·ND / 30), a partir del máximo de frames, el flag DF y el periodo medido. Solo cuenta frames consecutivos, para que la basura tras un corte no la confunda.
3. **Enganche** tras 3 frames consecutivos y el primer cambio de segundo. Al enganchar envía un **Full Frame** y empieza los Quarter Frames con la pieza 0 en frame par.
4. **PLL de 2º orden**: corrige la fase poco a poco y sigue la velocidad (varispeed hasta ±10 %). Si el error pasa de 1,5 frames, hace un *relocate* con un Full Frame nuevo.
5. **Freewheel** configurable: si el LTC se corta, sigue con el reloj propio y para cuando se acaba el tiempo de freewheel.
6. **Envío con hora exacta**:
   - Web: `MIDIOutput.send(data, timestamp)`.
   - App: CoreMIDI con `MIDITimeStamp`, sacado del reloj de la tarjeta de audio (`hostTimeNs`), con la latencia de entrada→salida compensada.

**Pruebas de la app (LtcTests):**
- Intervalo de Quarter Frames exacto en las 6 rates, con desviación ≤ 0,0004 ms.
- Fase del MTC respecto al LTC: 0,02–0,03 ms.
- Un corte de 200 ms lo cubre el freewheel.

## Fuentes
- Ableton Live 12 Manual — [Synchronizing with Link, Tempo Follower and MIDI](https://www.ableton.com/en/live-manual/12/synchronizing-with-link-tempo-follower-and-midi/)
- MOTU 1248 User Manual — [LTC-to-MTC Conversion (p. 80)](https://www.manualslib.com/manual/2908044/Motu-1248.html?page=80)
- MOTU 828es User Manual — [Syncing to SMPTE Time Code (p. 48)](https://www.manualslib.com/manual/1305776/Motu-828es.html?page=48)
- Sweetwater — [MOTU Issue: LTC QuickLock vs LTC](https://www.sweetwater.com/sweetcare/articles/motu-issue-ltc-quicklock-vs-ltc/)
