#!/bin/sh
set -e
. ./config.sh

mkdir -p "$SYSROOT"

for PROJECT in $SYSTEM_HEADER_PROJECTS; do
    echo "Installing headers for $PROJECT..."
    (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE install-headers)
done

# Копируем заголовки из kernel в правильную структуру
if [ -d "kernel" ]; then
    echo "Copying kernel headers..."
    mkdir -p "$SYSROOT/usr/include/kernel"
    
    # Основные заголовки
    cp -r kernel/include/* "$SYSROOT/usr/include/" 2>/dev/null || true
    
    # Заголовки из поддиректорий
    for dir in debug drivers mm task; do
        if [ -d "kernel/$dir" ]; then
            mkdir -p "$SYSROOT/usr/include/kernel/$dir"
            cp -r kernel/$dir/*.h "$SYSROOT/usr/include/kernel/$dir/" 2>/dev/null || true
            # Также копируем в корень для обратной совместимости
            cp kernel/$dir/*.h "$SYSROOT/usr/include/kernel/" 2>/dev/null || true
        fi
    done
fi

echo "Headers installed successfully"
