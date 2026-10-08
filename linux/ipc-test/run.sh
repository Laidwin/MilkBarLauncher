#!/usr/bin/env bash
# Builds the mod, a fake Cemu and the test launcher, then checks they talk over the socket.
# Needs g++, cmake and the .NET 8 SDK.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
dll="$here/../../DLL/InjectDLL"
build="$here/build"
mkdir -p "$build"

cmake -S "$dll" -B "$build/mod" >/dev/null
cmake --build "$build/mod" -j >/dev/null
g++ -O1 -no-pie -o "$build/Cemu" "$here/fake-cemu.cpp"

HOME="$build/home" dotnet run --project "$here/IpcTest.csproj" -- "$build/Cemu" "$build/mod/libbotwm.so"
