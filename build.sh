#!/bin/bash
echo "=== Сборка SolarDataCatcher ==="

mkdir -p build
cd build

echo "1. Генерируем Makefile..."
cmake .. -DCMAKE_BUILD_TYPE=Release

echo "2. Компилируем..."
make -j4

echo "3. Проверяем..."
if [ -f "solar-watcher" ]; then
    echo "Успешно! Исполняемый файл: build/solar-watcher"
    echo ""
    echo "Для установки выполните:"
    echo "  cp build/solar-watcher ."
    echo "  ./install.sh"
else
    echo "Ошибка сборки!"
fi