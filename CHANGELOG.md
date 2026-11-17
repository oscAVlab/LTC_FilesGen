# Cambios

## 2026-11-17
- **Web**: manual de marca osc[AV]lab V_2026 (logo oficial, paleta ink/paper/turquesa, Roboto + Roboto Mono). PR #1.
- **Web**: layout horizontal en desktop (3 columnas), 2 en tablet, apilado en móvil. PR #2.
- **Web**: nueva pestaña **LTC → MTC** (lector LTC por audio / generador → MTC regenerado por Web MIDI). Fix del generador en drop frame: los 2 primeros frames de cada bloque de 10 min salían mal codificados. PR #3.
- **App Mac osc[AV]lab LTC v0.1** (JUCE) en `app/`: LTC → MTC regenerado por CoreMIDI con timestamps, puerto virtual, generador con salida LTC y generador de archivos. Fixes en el lector web: fin de frame medio bit tarde, y un frame viejo que se colaba tras un corte. PR #4.
- Documentación: `docs/LTC-MTC-explicacion.md`, `docs/GUIA-web.md`, `app/README.md`.
