#!/usr/bin/env bash
# Starts native Linux Cemu with the mod (libbotwm.so) preloaded.
# Usage: ./run-cemu-with-mod.sh /path/to/Cemu [cemu args...]
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
dll="$here/../DLL/InjectDLL"
lib="$dll/build/libbotwm.so"

if [[ ! -f "$lib" ]]; then
    echo "Mod not built. Run: cmake -S \"$dll\" -B \"$dll/build\" && cmake --build \"$dll/build\"" >&2
    exit 1
fi

cemu="${1:?usage: $0 /path/to/Cemu [cemu args...]}"
shift

exec env LD_PRELOAD="$lib${LD_PRELOAD:+:$LD_PRELOAD}" "$cemu" "$@"
