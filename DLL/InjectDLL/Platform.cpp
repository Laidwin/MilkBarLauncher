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
    memory_getBaseType memory_getBase = (memory_getBaseType)FindCemuSymbol("memory_getBase");
    return memory_getBase ? (uint64_t)memory_getBase() : 0;
}

uint64_t Platform::FindCemuSymbol(const char* name)
{
    return (uint64_t)GetProcAddress(GetModuleHandleA("Cemu.exe"), name);
}

bool Platform::RegisterHLEFunction(const char* libraryName, const char* functionName, void* function)
{
    typedef void (*osLib_registerHLEFunctionType)(const char*, const char*, void*);
    osLib_registerHLEFunctionType osLib_registerHLEFunction = (osLib_registerHLEFunctionType)FindCemuSymbol("osLib_registerHLEFunction");
    if (!osLib_registerHLEFunction)
        return false;

    osLib_registerHLEFunction(libraryName, functionName, function);
    return true;
}

bool Platform::IsCemuProcess()
{
    return GetModuleHandleA("Cemu.exe") != nullptr;
}

void Platform::WaitForCemuStartup()
{
    // The launcher injects the DLL into an already running Cemu.
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
#include <elf.h>
#include <link.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <thread>
#include <unordered_map>

namespace
{
    // Load bias of the main executable (0 for a non-PIE build, like official Cemu releases).
    uint64_t ExecutableLoadBias()
    {
        uint64_t bias = 0;
        dl_iterate_phdr([](dl_phdr_info* info, size_t, void* data) {
            *static_cast<uint64_t*>(data) = info->dlpi_addr;
            return 1; // The main executable is reported first.
        }, &bias);
        return bias;
    }

    // Functions and variables defined in the executable's .symtab. Linux Cemu exports nothing,
    // but release builds keep this table (they only strip debug info).
    std::unordered_map<std::string, uint64_t> ReadExecutableSymbols()
    {
        std::unordered_map<std::string, uint64_t> result;
        std::ifstream file("/proc/self/exe", std::ios::binary);

        Elf64_Ehdr header{};
        if (!file.read(reinterpret_cast<char*>(&header), sizeof(header)) || memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 || header.e_ident[EI_CLASS] != ELFCLASS64)
            return result;

        std::vector<Elf64_Shdr> sections(header.e_shnum);
        file.seekg(header.e_shoff);
        if (!file.read(reinterpret_cast<char*>(sections.data()), sections.size() * sizeof(Elf64_Shdr)))
            return result;

        uint64_t bias = ExecutableLoadBias();
        for (const Elf64_Shdr& section : sections)
        {
            if (section.sh_type != SHT_SYMTAB || section.sh_link >= sections.size())
                continue;

            const Elf64_Shdr& namesSection = sections[section.sh_link];
            std::vector<char> names(namesSection.sh_size);
            file.seekg(namesSection.sh_offset);
            std::vector<Elf64_Sym> symbols(section.sh_size / sizeof(Elf64_Sym));
            if (!file.read(names.data(), names.size()))
                return result;
            file.seekg(section.sh_offset);
            if (!file.read(reinterpret_cast<char*>(symbols.data()), symbols.size() * sizeof(Elf64_Sym)) || names.empty() || names.back() != '\0')
                return result;

            for (const Elf64_Sym& symbol : symbols)
            {
                int type = ELF64_ST_TYPE(symbol.st_info);
                if (symbol.st_shndx == SHN_UNDEF || (type != STT_FUNC && type != STT_OBJECT) || symbol.st_name >= names.size())
                    continue;
                result.emplace(&names[symbol.st_name], bias + symbol.st_value);
            }
        }
        return result;
    }

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
    if (memory_getBaseType memory_getBase = (memory_getBaseType)FindCemuSymbol("memory_getBase"))
        return (uint64_t)memory_getBase();

    // What memory_getBase returns: `uint8* memory_base` in Cemu's MMU.cpp, set by memory_init().
    if (uint64_t memoryBase = FindCemuSymbol("memory_base"))
    {
        uint64_t base = 0;
        ReadMemory(memoryBase, &base, sizeof(base));
        return base;
    }

    return FindBaseFromLayout();
}

uint64_t Platform::FindCemuSymbol(const char* name)
{
    if (void* exported = dlsym(RTLD_DEFAULT, name))
        return (uint64_t)exported;

    static const std::unordered_map<std::string, uint64_t> symbols = ReadExecutableSymbols();
    auto found = symbols.find(name);
    return found != symbols.end() ? found->second : 0;
}

bool Platform::RegisterHLEFunction(const char* libraryName, const char* functionName, void* function)
{
    // osLib_registerHLEFunction only forwards to osLib_addFunctionInternal, and the linker drops it
    // on Linux because nothing in Cemu calls it. Both share the same signature.
    typedef void (*osLib_registerHLEFunctionType)(const char*, const char*, void*);
    osLib_registerHLEFunctionType osLib_registerHLEFunction = (osLib_registerHLEFunctionType)FindCemuSymbol("osLib_registerHLEFunction");
    if (!osLib_registerHLEFunction)
        osLib_registerHLEFunction = (osLib_registerHLEFunctionType)FindCemuSymbol("_Z25osLib_addFunctionInternalPKcS0_PFvP16PPCInterpreter_tE");
    if (!osLib_registerHLEFunction)
        return false;

    osLib_registerHLEFunction(libraryName, functionName, function);
    return true;
}

bool Platform::IsCemuProcess()
{
    char path[4096];
    ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (length <= 0)
        return false;
    path[length] = '\0';

    std::string name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
    return name.find("cemu") != std::string::npos;
}

void Platform::WaitForCemuStartup()
{
    // Cemu registers its HLE functions while initializing (before any title boots and applies
    // graphic pack patches) in `std::vector<osFunctionEntry_t>* s_osFunctionTable`. Wait until
    // that vector exists and stops growing, so our registration doesn't race Cemu's.
    uint64_t tableSymbol = FindCemuSymbol("s_osFunctionTable");
    if (!tableSymbol)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return;
    }

    uint64_t lastEnd = 0;
    int stableChecks = 0;
    while (stableChecks < 5)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // libstdc++ vector layout: begin, end, capacity pointers.
        uint64_t table = 0;
        uint64_t end = 0;
        if (!ReadMemory(tableSymbol, &table, sizeof(table)) || table == 0 || !ReadMemory(table + sizeof(uint64_t), &end, sizeof(end)))
            continue;

        stableChecks = (end == lastEnd) ? stableChecks + 1 : 0;
        lastEnd = end;
    }
}

std::string Platform::AppDataDirectory()
{
    if (const char* config = getenv("XDG_CONFIG_HOME"); config && *config)
        return config;

    const char* home = getenv("HOME");
    return std::string(home ? home : "") + "/.config";
}

#endif
