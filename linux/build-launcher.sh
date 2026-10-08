#!/usr/bin/env bash
# Builds the mod (libbotwm.so) and the Avalonia launcher into build/launcher, with the mod next to
# the launcher executable where it looks for it. Needs g++, cmake and the .NET 8 SDK.
# Usage: ./linux/build-launcher.sh, then ./build/launcher/MilkBarLauncher
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$root/build/launcher"

git -C "$root" submodule update --init Avalonia/External/BfresLibrary

cmake -S "$root/DLL/InjectDLL" -B "$root/DLL/InjectDLL/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/DLL/InjectDLL/build" -j

dotnet publish "$root/Avalonia/Launcher/MilkBarLauncher.csproj" -c Release -r linux-x64 --self-contained true -o "$out"
cp "$root/DLL/InjectDLL/build/libbotwm.so" "$out/"

echo "Launcher ready: $out/MilkBarLauncher"
