// botwm-probe: feasibility probe for running the Milk Bar Launcher mod on native Linux Cemu.
//
// Loaded into Cemu with LD_PRELOAD, it checks the three things the real mod depends on:
//   1. finding the base of Cemu's emulated Wii U memory (memory_getBase, like ReadCemu.cpp),
//   2. pattern scanning guest memory (the LocationAccess signature from LocationAccess.h),
//   3. reading live game data (map / section names, which change as Link moves).
// It also looks for the JIT glyph signature from Main::glyphScan, but never patches anything.
// Everything is read-only: the probe never writes to Cemu's memory.

#include <dlfcn.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

// Cemu reserves the whole 32-bit Wii U address space up front and commits pieces of it later.
constexpr uint64_t kGuestSpaceSize = 0x100000000ULL;

// DataTypes::LocationAccess (DLL/InjectDLL/LocationAccess.h). Map name at +0x14, section at +0x40.
const std::vector<int> kLocationSig = { 0x03, -1, 0xB5, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x10, 0x54, 0xF6, 0x44, 0x10, 0x2B, 0x7E, 0x78, 0x00, 0x00, 0x00 };
constexpr uint64_t kMapOffset = 0x14;
constexpr uint64_t kSectionOffset = 0x40;

// Main::glyphScan (DLL/InjectDLL/dllmain_Functions.cpp). Lives in Cemu's x64 JIT output, not in guest memory.
const std::vector<int> kGlyphSig = { 0x45, 0x0F, 0x38, 0xF0, 0x74, 0x2D, 0x18, 0x66, 0x41, 0x0F, 0x6E, 0xC6, -1, 0x7C, 0x24, 0x08 };

constexpr size_t kMaxHits = 16;

struct Region
{
    uint64_t start = 0;
    uint64_t end = 0;
    std::string perms;
    std::string path;

    bool readable() const { return !perms.empty() && perms[0] == 'r'; }
    bool executable() const { return perms.size() > 2 && perms[2] == 'x'; }
};

FILE* g_log = nullptr;

void log(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    fprintf(stderr, "[botwm-probe] %s\n", buf);
    if (g_log)
    {
        fprintf(g_log, "%s\n", buf);
        fflush(g_log);
    }
}

void sleepMs(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

std::string exePath()
{
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return "";
    buf[n] = '\0';
    return buf;
}

// LD_PRELOAD is inherited by everything Cemu spawns, so only run inside Cemu itself.
bool isCemuProcess()
{
    std::string exe = exePath();
    std::string name = exe.substr(exe.find_last_of('/') + 1);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
    return name.find("cemu") != std::string::npos;
}

std::vector<Region> readMaps()
{
    std::vector<Region> result;
    std::ifstream maps("/proc/self/maps");
    std::string line;

    while (std::getline(maps, line))
    {
        unsigned long long start = 0, end = 0;
        char perms[8] = {};
        int pathPos = static_cast<int>(line.size());

        if (sscanf(line.c_str(), "%llx-%llx %7s %*s %*s %*s %n", &start, &end, perms, &pathPos) < 3)
            continue;

        Region region;
        region.start = start;
        region.end = end;
        region.perms = perms;
        if (pathPos < static_cast<int>(line.size()))
            region.path = line.substr(pathPos);
        result.push_back(region);
    }

    return result;
}

// Reads through the kernel so that a bad pointer returns false instead of crashing Cemu.
bool safeRead(uint64_t addr, void* dst, size_t len)
{
    iovec local{ dst, len };
    iovec remote{ reinterpret_cast<void*>(addr), len };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == static_cast<ssize_t>(len);
}

// Same rules as Memory::extractLocName: stop at the first space (or NUL).
std::string readLocName(uint64_t addr)
{
    char buf[50] = {};
    if (!safeRead(addr, buf, sizeof(buf)))
        return "<unreadable>";

    std::string result;
    for (char c : buf)
    {
        if (c == ' ' || c == '\0')
            break;
        result += std::isprint(static_cast<unsigned char>(c)) ? c : '?';
    }
    return result;
}

uint64_t parseHexEnv(const char* name)
{
    const char* value = getenv(name);
    return value ? strtoull(value, nullptr, 16) : 0;
}

// Cemu's 4 GiB reservation shows up as contiguous anonymous mappings spanning at least 4 GiB.
std::vector<uint64_t> baseCandidatesFromMaps(const std::vector<Region>& maps)
{
    std::vector<uint64_t> result;

    for (size_t i = 0; i < maps.size();)
    {
        if (!maps[i].path.empty())
        {
            i++;
            continue;
        }

        uint64_t runStart = maps[i].start;
        uint64_t runEnd = maps[i].end;
        size_t j = i + 1;
        while (j < maps.size() && maps[j].path.empty() && maps[j].start == runEnd)
            runEnd = maps[j++].end;

        if (runEnd - runStart >= kGuestSpaceSize)
            result.push_back(runStart);
        i = j;
    }

    return result;
}

uint64_t waitForBase()
{
    using MemoryGetBase = void* (*)();
    auto memoryGetBase = reinterpret_cast<MemoryGetBase>(dlsym(RTLD_DEFAULT, "memory_getBase"));

    if (memoryGetBase)
        log("memory_getBase is exported by Cemu (good: same method as the Windows DLL)");
    else
        log("memory_getBase is NOT exported, falling back to scanning /proc/self/maps");

    for (int attempt = 0;; attempt++)
    {
        if (uint64_t base = parseHexEnv("BOTWM_BASE"))
        {
            log("Using BOTWM_BASE override: 0x%" PRIx64, base);
            return base;
        }

        if (memoryGetBase)
        {
            if (uint64_t base = reinterpret_cast<uint64_t>(memoryGetBase()))
            {
                log("Base from memory_getBase: 0x%" PRIx64, base);
                return base;
            }
        }
        else
        {
            std::vector<uint64_t> candidates = baseCandidatesFromMaps(readMaps());
            if (candidates.size() == 1)
            {
                log("Base from /proc/self/maps heuristic: 0x%" PRIx64, candidates[0]);
                return candidates[0];
            }
            if (candidates.size() > 1 && attempt % 15 == 0)
            {
                for (uint64_t candidate : candidates)
                    log("  base candidate: 0x%" PRIx64, candidate);
                log("Several base candidates, set BOTWM_BASE=<hex> to pick one");
            }
        }

        if (attempt % 15 == 0)
            log("Waiting for Cemu to allocate guest memory (start the game)...");
        sleepMs(2000);
    }
}

// Readable parts of the guest address space, clipped to [base, base + 4 GiB).
std::vector<Region> guestRegions(const std::vector<Region>& maps, uint64_t base)
{
    std::vector<Region> result;
    for (Region region : maps)
    {
        if (!region.readable() || region.end <= base || region.start >= base + kGuestSpaceSize)
            continue;
        region.start = std::max(region.start, base);
        region.end = std::min(region.end, base + kGuestSpaceSize);
        result.push_back(region);
    }
    return result;
}

std::vector<Region> jitRegions(const std::vector<Region>& maps, uint64_t base)
{
    std::vector<Region> result;
    for (const Region& region : maps)
    {
        bool inGuest = region.end > base && region.start < base + kGuestSpaceSize;
        if (region.readable() && region.executable() && !inGuest)
            result.push_back(region);
    }
    return result;
}

bool matchesAt(const uint8_t* p, const std::vector<int>& sig)
{
    for (size_t i = 0; i < sig.size(); i++)
        if (sig[i] != -1 && p[i] != static_cast<uint8_t>(sig[i]))
            return false;
    return true;
}

// Finds the longest wildcard-free run of the signature, memmem()s for it, then checks the rest.
std::vector<uint64_t> scan(const std::vector<int>& sig, const std::vector<Region>& regions)
{
    size_t anchorOffset = 0, anchorLength = 0;
    for (size_t i = 0; i < sig.size();)
    {
        if (sig[i] == -1)
        {
            i++;
            continue;
        }
        size_t j = i;
        while (j < sig.size() && sig[j] != -1)
            j++;
        if (j - i > anchorLength)
        {
            anchorOffset = i;
            anchorLength = j - i;
        }
        i = j;
    }

    std::vector<uint8_t> anchor;
    for (size_t i = anchorOffset; i < anchorOffset + anchorLength; i++)
        anchor.push_back(static_cast<uint8_t>(sig[i]));

    std::vector<uint64_t> hits;
    for (const Region& region : regions)
    {
        auto* begin = reinterpret_cast<const uint8_t*>(region.start);
        auto* end = reinterpret_cast<const uint8_t*>(region.end);
        auto* cursor = begin;

        while (cursor + anchorLength <= end)
        {
            auto* found = static_cast<const uint8_t*>(memmem(cursor, end - cursor, anchor.data(), anchor.size()));
            if (!found)
                break;

            const uint8_t* candidate = found - anchorOffset;
            if (candidate >= begin && candidate + sig.size() <= end && matchesAt(candidate, sig))
            {
                hits.push_back(reinterpret_cast<uint64_t>(candidate));
                if (hits.size() >= kMaxHits)
                    return hits;
            }
            cursor = found + 1;
        }
    }
    return hits;
}

// Index among readable guest regions, to compare with the Windows "region 8" convention.
int regionIndexOf(uint64_t addr, const std::vector<Region>& regions)
{
    for (size_t i = 0; i < regions.size(); i++)
        if (addr >= regions[i].start && addr < regions[i].end)
            return static_cast<int>(i) + 1;
    return -1;
}

void dumpGuestLayout(const std::vector<Region>& regions, uint64_t base)
{
    log("Readable guest regions (guest address = host - base):");
    for (size_t i = 0; i < regions.size(); i++)
        log("  #%-3zu guest 0x%08" PRIx64 "-0x%08" PRIx64 " (%6" PRIu64 " KiB) %s",
            i + 1, regions[i].start - base, regions[i].end - base,
            (regions[i].end - regions[i].start) / 1024, regions[i].perms.c_str());
}

void run()
{
    log("Loaded into %s (pid %d)", exePath().c_str(), getpid());

    uint64_t base = waitForBase();

    std::vector<Region> guest;
    std::vector<uint64_t> hits;
    for (int attempt = 0; hits.empty(); attempt++)
    {
        if (attempt > 0)
            sleepMs(2000);
        if (attempt % 15 == 0)
            log("Scanning guest memory for the location signature (load a save with the mod and graphic packs enabled)...");

        guest = guestRegions(readMaps(), base);
        hits = scan(kLocationSig, guest);
    }

    dumpGuestLayout(guest, base);

    log("Location signature: %zu hit(s)%s", hits.size(), hits.size() >= kMaxHits ? " (truncated)" : "");
    for (size_t i = 0; i < hits.size(); i++)
        log("  hit %zu: host 0x%" PRIx64 " guest 0x%08" PRIx64 " region #%d map='%s' section='%s'",
            i, hits[i], hits[i] - base, regionIndexOf(hits[i], guest),
            readLocName(hits[i] + kMapOffset).c_str(), readLocName(hits[i] + kSectionOffset).c_str());

    std::vector<uint64_t> glyphHits = scan(kGlyphSig, jitRegions(readMaps(), base));
    log("Glyph JIT signature: %zu hit(s) (not patched, informational only)", glyphHits.size());
    for (uint64_t hit : glyphHits)
        log("  glyph hit: host 0x%" PRIx64, hit);

    log("Watching map/section names. Walk around in game: they should change.");
    std::vector<std::string> last(hits.size());
    for (;;)
    {
        for (size_t i = 0; i < hits.size(); i++)
        {
            std::string current = readLocName(hits[i] + kMapOffset) + " / " + readLocName(hits[i] + kSectionOffset);
            if (current != last[i])
            {
                log("hit %zu: %s", i, current.c_str());
                last[i] = current;
            }
        }
        sleepMs(500);
    }
}

__attribute__((constructor)) void onLoad()
{
    if (!isCemuProcess())
        return;

    std::string logPath = getenv("BOTWM_PROBE_LOG") ? getenv("BOTWM_PROBE_LOG") : "/tmp/botwm-probe-" + std::to_string(getpid()) + ".log";
    g_log = fopen(logPath.c_str(), "w");
    log("Logging to %s", logPath.c_str());

    std::thread(run).detach();
}

}
