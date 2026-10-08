#!/usr/bin/env bash
# Starts Cemu with the memory-layer test preloaded.
# Usage: ./run-cemu.sh /path/to/Cemu [cemu args...]
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
lib="$here/build/libbotwm_memtest.so"

if [[ ! -f "$lib" ]]; then
    echo "Not built. Run: cmake -S \"$here\" -B \"$here/build\" && cmake --build \"$here/build\"" >&2
    exit 1
fi

cemu="${1:?usage: $0 /path/to/Cemu [cemu args...]}"
shift

exec env LD_PRELOAD="$lib${LD_PRELOAD:+:$LD_PRELOAD}" "$cemu" "$@"
