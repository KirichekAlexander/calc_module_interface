#!/usr/bin/env bash
set -euo pipefail

APP_NAME=calc_module_interface

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Собираем tar.gz с бинарником
mkdir -p /out
if [ -f "build/${APP_NAME}" ]; then
  tar -czf "/out/${APP_NAME}-astra.tar.gz" -C build "${APP_NAME}"
else
  echo "Не найден build/${APP_NAME}. Вот что есть в build/:"
  ls -la build
  exit 1
fi
