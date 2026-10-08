#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// OS-specific process memory and environment access. Implemented in Platform.cpp for
// Windows (Cemu.exe, DLL injection) and Linux (native Cemu, LD_PRELOAD).
namespace Platform
{
	struct MemoryRegion
	{
		uint64_t start;
		uint64_t size;
		bool readable;
	};

	// Consecutive regions starting with the one containing `from`, in VirtualQuery order:
	// uncommitted gaps are regions too, so callers can count them ("region 8" is the same
	// guest range on both platforms). Stops after maxRegions entries.
	std::vector<MemoryRegion> EnumerateRegions(uint64_t from, size_t maxRegions = SIZE_MAX);

	// The region containing address. False if the address is outside the user address space.
	bool QueryRegion(uint64_t address, MemoryRegion& region);

	// Copies process memory, failing instead of crashing on unmapped or protected addresses
	// on Linux. On Windows this is a plain copy, as before.
	bool ReadMemory(uint64_t address, void* buffer, size_t size);
	bool WriteMemory(uint64_t address, const void* buffer, size_t size);

	// Host address of the emulated Wii U address space, or 0 if Cemu hasn't allocated it yet.
	uint64_t FindCemuMemoryBase();

	// Per-user settings folder, matching .NET's SpecialFolder.ApplicationData used by the launcher:
	// %APPDATA% on Windows, $XDG_CONFIG_HOME (or ~/.config) on Linux.
	std::string AppDataDirectory();
}
