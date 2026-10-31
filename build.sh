#!/bin/sh
# Build Headphones Guy with MinGW-w64 (GCC 13 win32 threads works fine).
#   Linux / WSL:     sudo apt install g++-mingw-w64-x86-64-win32 binutils-mingw-w64-x86-64
#   Windows (MSYS2): pacman -S mingw-w64-x86_64-gcc   then: CXX=g++ WINDRES=windres sh build.sh
set -e
cd "$(dirname "$0")"
CXX=${CXX:-x86_64-w64-mingw32-g++}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
mkdir -p build
V=$(tr -d ' \r\n' < VERSION)
V1=$(echo "$V" | cut -d. -f1); V2=$(echo "$V" | cut -d. -f2); V3=$(echo "$V" | cut -d. -f3)
$WINDRES -DHG_V1=$V1 -DHG_V2=$V2 -DHG_V3=$V3 "-DHG_VS=\\\"$V\\\"" src/res.rc -O coff -o build/res.o
$CXX -O2 -s -std=c++17 -Wall -municode -mwindows -static src/main.cpp build/res.o -o build/HeadphonesGuy.exe \
     -lole32 -luuid -lshell32 -lcomdlg32 -lws2_32
echo "build/HeadphonesGuy.exe is ready"
