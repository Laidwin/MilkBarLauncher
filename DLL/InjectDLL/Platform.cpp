#include "Platform.h"
#include "Compat.h"

#include <cstdlib>
#include <cstring>

#ifdef _WIN32

namespace
{
    bool IsReadable(const MEMORY_BASIC_INFORMATION& mbi)
    {
        DWORD protectflags = (PAGE_GUARD | PAGE_NOCACHE | PAGE_NOACCESS);
        return !(mbi.Protect & protectflags) && (mbi.State & MEM_COMMIT);
    }
}

std::vector<Platform::MemoryRegion> Platform::EnumerateRegions(uint64_t from, size_t maxRegions)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uint64_t endAddress = (uint64_t)(si.lpMaximumApplicationAddress);

    std::vector<MemoryRegion> regions;
    MEMORY_BASIC_INFORMATION mbi{ 0 };

    for (uint64_t address = from; address < endAddress && regions.size() < maxRegions;)
    {
        if (!VirtualQuery((LPCVOID)address, &mbi, sizeof(mbi)))
            break;

        regions.push_back({ (uint64_t)mbi.BaseAddress, mbi.RegionSize, IsReadable(mbi) });
        address = (uint64_t)mbi.BaseAddress + mbi.RegionSize;
    }

    return regions;
}

bool Platform::QueryRegion(uint64_t address, MemoryRegion& region)
{
    MEMORY_BASIC_INFORMATION mbi{ 0 };
    if (!VirtualQuery((LPCVOID)address, &mbi, sizeof(mbi)))
        return false;

    region = { (uint64_t)mbi.BaseAddress, mbi.RegionSize, IsReadable(mbi) };
    return true;
}

bool Platform::ReadMemory(uint64_t address, void* buffer, size_t size)
{
    memcpy(buffer, (void*)address, size);
    return true;
}

bool Platform::WriteMemory(uint64_t address, const void* buffer, size_t size)
{
    memcpy((void*)address, buffer, size);
    return true;
}

uint64_t Platform::FindCemuMemoryBase()
{
    typedef void* (*memory_getBaseType)();
    memory_getBaseType memory_getBase = (memory_getBaseType)GetProcAddress(GetModuleHandleA("Cemu.exe"), "memory_getBase");
    return memory_getBase ? (uint64_t)memory_getBase() : 0;
}

std::string Platform::AppDataDirectory()
{
    char* appdata = nullptr;
    size_t size = 0;
    _dupenv_s(&appdata, &size, "APPDATA");

    std::string result = appdata ? appdata : "";
    free(appdata);
    return result;
}

#else

#include <dlfcn.h>
#include <sys/uio.h>
#include <unistd.h>

#include <fstream>

namespace
{
    // Above any user address (47/48-bit, or 56-bit with 5-level paging), below [vsyscall].
    constexpr uint64_t kMaxUserAddress = 1ULL << 56;

    struct Mapping
    {
        uint64_t start;
        uint64_t end;
        std::string perms;
        std::string path;
    };

    std::vector<Mapping> ReadMappings()
    {
        std::vector<Mapping> result;
        std::ifstream maps("/proc/self/maps");
        std::string line;

        while (std::getline(maps, line))
        {
            unsigned long long start = 0, end = 0;
            char perms[8] = {};
            int pathPos = static_cast<int>(line.size());

            if (sscanf(line.c_str(), "%llx-%llx %7s %*s %*s %*s %n", &start, &end, perms, &pathPos) < 3)
                continue;

            Mapping mapping{ start, end, perms, "" };
            if (pathPos < static_cast<int>(line.size()))
                mapping.path = line.substr(pathPos);
            result.push_back(mapping);
        }

        return result;
    }

    // Device and kernel mappings can fault or have side effects when scanned, so treat them like
    // Windows treats PAGE_NOACCESS memory.
    bool IsReadable(const Mapping& mapping)
    {
        if (mapping.perms.empty() || mapping.perms[0] != 'r')
            return false;
        return mapping.path.rfind("/dev/", 0) != 0 && mapping.path != "[vvar]" && mapping.path != "[vsyscall]";
    }

    // The whole user address space as VirtualQuery would describe it: unmapped gaps become
    // unreadable regions, and neighbouring mappings with the same readability are merged.
    // Linux splits mappings much more freely than Windows (e.g. per mprotect call), so without
    // merging the region numbers used by the scanner would drift.
    std::vector<Platform::MemoryRegion> AllRegions()
    {
        std::vector<Platform::MemoryRegion> regions;

        auto append = [&regions](uint64_t start, uint64_t end, bool readable) {
            if (!regions.empty() && regions.back().readable == readable && regions.back().start + regions.back().size == start)
                regions.back().size += end - start;
            else
                regions.push_back({ start, end - start, readable });
        };

        uint64_t cursor = 0;
        for (const Mapping& mapping : ReadMappings())
        {
            if (mapping.start >= kMaxUserAddress)
                break;
            if (mapping.start > cursor)
                append(cursor, mapping.start, false);
            append(mapping.start, mapping.end, IsReadable(mapping));
            cursor = mapping.end;
        }
        if (cursor < kMaxUserAddress)
            append(cursor, kMaxUserAddress, false);

        return regions;
    }

    // True if every byte of [start, end) is readable (mappings are sorted by address).
    bool RangeReadable(const std::vector<Mapping>& mappings, uint64_t start, uint64_t end)
    {
        uint64_t cursor = start;
        for (const Mapping& mapping : mappings)
        {
            if (mapping.end <= cursor)
                continue;
            if (mapping.start > cursor || !IsReadable(mapping))
                return false;
            cursor = mapping.end;
            if (cursor >= end)
                return true;
        }
        return false;
    }

    // Cemu on Linux doesn't export memory_getBase, so find the guest base from the fixed ranges
    // Cemu commits once a title boots, including the uncommitted gaps between them. Weaker checks
    // fail: the 4 GiB reservation merges with neighbouring anonymous mappings, and glibc's 64 MiB
    // aligned malloc arenas match "a mapping starts at X" tests. Validated on Cemu 2.0-48.
    uint64_t FindBaseFromLayout()
    {
        constexpr uint64_t kPage = 0x1000;
        constexpr uint64_t kMem1 = 0xF4000000;

        std::vector<Mapping> mappings = ReadMappings();

        auto matchesLayout = [&mappings](uint64_t base) {
            return RangeReadable(mappings, base + 0x10000000, base + 0x50000000)    // MEM2 (1 GiB minimum)
                && RangeReadable(mappings, base + 0xE0000000, base + 0xE4000000)    // FG bucket
                && !RangeReadable(mappings, base + 0xE4000000, base + 0xE4000000 + kPage)
                && RangeReadable(mappings, base + 0xE8000000, base + 0xEA000000)    // tiling aperture
                && !RangeReadable(mappings, base + 0xEA000000, base + 0xEA000000 + kPage)
                && RangeReadable(mappings, base + kMem1, base + 0xFA000000);        // MEM1, RPL loader, shared
        };

        uint64_t found = 0;
        for (const Mapping& mapping : mappings)
        {
            // MEM1 follows an uncommitted gap, so it always starts its own mapping.
            if (!IsReadable(mapping) || !mapping.path.empty() || mapping.start < kMem1)
                continue;

            uint64_t base = mapping.start - kMem1;
            if (matchesLayout(base))
            {
                if (found != 0)
                    return 0; // Ambiguous: wait rather than guess.
                found = base;
            }
        }
        return found;
    }
}

std::vector<Platform::MemoryRegion> Platform::EnumerateRegions(uint64_t from, size_t maxRegions)
{
    std::vector<MemoryRegion> result;
    for (const MemoryRegion& region : AllRegions())
    {
        if (result.size() >= maxRegions)
            break;
        if (region.start + region.size > from)
            result.push_back(region);
    }
    return result;
}

bool Platform::QueryRegion(uint64_t address, MemoryRegion& region)
{
    std::vector<MemoryRegion> regions = EnumerateRegions(address, 1);
    if (regions.empty())
        return false;

    region = regions[0];
    return true;
}

bool Platform::ReadMemory(uint64_t address, void* buffer, size_t size)
{
    iovec local{ buffer, size };
    iovec remote{ reinterpret_cast<void*>(address), size };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == static_cast<ssize_t>(size);
}

bool Platform::WriteMemory(uint64_t address, const void* buffer, size_t size)
{
    iovec local{ const_cast<void*>(buffer), size };
    iovec remote{ reinterpret_cast<void*>(address), size };
    return process_vm_writev(getpid(), &local, 1, &remote, 1, 0) == static_cast<ssize_t>(size);
}

uint64_t Platform::FindCemuMemoryBase()
{
    typedef void* (*memory_getBaseType)();
    memory_getBaseType memory_getBase = (memory_getBaseType)dlsym(RTLD_DEFAULT, "memory_getBase");
    if (memory_getBase)
        return (uint64_t)memory_getBase();

    return FindBaseFromLayout();
}

std::string Platform::AppDataDirectory()
{
    if (const char* config = getenv("XDG_CONFIG_HOME"); config && *config)
        return config;

    const char* home = getenv("HOME");
    return std::string(home ? home : "") + "/.config";
}

#endif
