# LTC FilesGen · osc[AV]lab

Herramientas de timecode SMPTE/LTC de osc[AV]lab.

| Qué | Dónde |
|---|---|
| Web: generar archivos WAV de LTC | https://oscavlab.github.io/LTC_FilesGen/ (`index.html`) |
| Web: LTC → MTC por Web MIDI | https://oscavlab.github.io/LTC_FilesGen/mtc.html |
| App Mac osc[AV]lab LTC (JUCE) | [`app/`](app/README.md) |
| Cómo funciona LTC/MTC y por qué la MOTU engancha Live | [`docs/LTC-MTC-explicacion.md`](docs/LTC-MTC-explicacion.md) |
| Guía de uso de la web | [`docs/GUIA-web.md`](docs/GUIA-web.md) |
| Historial | [`CHANGELOG.md`](CHANGELOG.md) |

## Instalar la app en el Mac
Requisitos (una vez): `xcode-select --install` y `brew install cmake`
```zsh
git clone https://github.com/oscAVlab/LTC_FilesGen.git "$HOME/_LAB/Devices/LTC"
zsh "$HOME/_LAB/Devices/LTC/app/mac/Compilar.command"
```
Para actualizar: `zsh "$HOME/_LAB/Devices/LTC/app/mac/Actualizar.command"`
