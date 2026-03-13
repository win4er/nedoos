#!/bin/bash
qemu-system-i386 -boot order=d -cdrom myos.iso -drive file=hd.img,format=raw,if=ide
