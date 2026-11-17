#!/bin/zsh
# osc[AV]lab LTC — traer la última versión de GitHub, compilar e instalar
set -e
cd "$(dirname "$0")/.."
osascript -e 'quit app "osc[AV]lab LTC"' 2>/dev/null || true
git pull
zsh mac/Compilar.command
