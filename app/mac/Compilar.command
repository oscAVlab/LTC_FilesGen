#!/bin/zsh
# osc[AV]lab LTC — compilar e instalar en /Applications
# Requisitos (una vez): xcode-select --install ; brew install cmake
set -e
cd "$(dirname "$0")/.."
echo "== osc[AV]lab LTC: configurando (la primera vez descarga JUCE, 1-2 min) =="
cmake -B build -DCMAKE_BUILD_TYPE=Release
echo "== compilando =="
cmake --build build --config Release --target OscAVlabLTC -j "$(sysctl -n hw.ncpu)"
APP="build/OscAVlabLTC_artefacts/Release/osc[AV]lab LTC.app"
echo "== instalando en /Applications =="
rm -rf "/Applications/osc[AV]lab LTC.app"
ditto "$APP" "/Applications/osc[AV]lab LTC.app"
echo "Listo. Abriendo osc[AV]lab LTC..."
open "/Applications/osc[AV]lab LTC.app"
