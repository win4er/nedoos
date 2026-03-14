#!/bin/sh
set -e

echo "Cleaning project..."

# Удаляем скомпилированные файлы
find . -type f -name "*.o" -delete
find . -type f -name "*.d" -delete
find . -type f -name "*.a" -delete
find . -type f -name "*.kernel" -delete
find . -type f -name "*.iso" -delete

# Удаляем директории
rm -rf sysroot
rm -rf isodir

echo "Clean completed"
