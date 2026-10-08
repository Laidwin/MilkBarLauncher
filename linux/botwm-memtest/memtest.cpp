// botwm-memtest: runs the mod's own memory layer (Platform.cpp, Scanner.cpp, ReadCemu.cpp from
// DLL/InjectDLL) inside native Linux Cemu, with the same calls and signatures the mod uses.
// It validates the Linux port of that layer before the whole mod is built as a .so.
// The only write puts back a byte that was just read, to exercise the write path.

#include "Memory.h"
#include "Platform.h"

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace {

FILE* g_log = nullptr;
int g_failures = 0;

void log(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    fprintf(stderr, "[botwm-memtest] %s\n", buf);
    if (g_log)
    {
        fprintf(g_log, "%s\n", buf);
        fflush(g_log);
    }
}

void check(bool ok, const std::string& what)
{
    if (!ok)
        g_failures++;
    log("[%s] %s", ok ? "OK" : "FAIL", what.c_str());
}

std::string hex(uint64_t value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%" PRIx64, value);
    return buf;
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

// Same retry policy as the mod: a scan returning < 30000 means "not found".
uint64_t scanUntilFound(const std::vector<int>& sig, int region, uint64_t offset, bool multipleRegions, const char* name)
{
    for (int attempt = 0;; attempt++)
    {
        uint64_t addr = Memory::PatternScan(sig, Memory::getBaseAddress(), region, offset, false, multipleRegions);
        if (addr >= 30000)
            return addr;
        if (attempt % 15 == 0)
            log("Scanning for %s (load a save with the mod and graphic packs enabled)...", name);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void run()
{
    log("Loaded into %s (pid %d)", exePath().c_str(), getpid());

    Logging::LoggerService::StartLoggerService();
    log("Mod log: %s/BOTWM/LatestLog.txt", Platform::AppDataDirectory().c_str());

    uint64_t base = 0;
    for (int attempt = 0; (base = Memory::getBaseAddress()) == 0; attempt++)
    {
        if (attempt % 15 == 0)
            log("Waiting for Cemu to allocate guest memory (start the game)...");
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    log("Base: %s", hex(base).c_str());

    // Cemu sets memory_base at startup but commits guest memory only when a title boots.
    Platform::MemoryRegion bootRegion{};
    for (int attempt = 0; !Platform::QueryRegion(base + 0x02000000, bootRegion) || !bootRegion.readable; attempt++)
    {
        if (attempt % 15 == 0)
            log("Waiting for the game to boot...");
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    std::vector<Platform::MemoryRegion> regions = Platform::EnumerateRegions(base, 12);
    log("First regions from base, as the scanner numbers them:");
    for (size_t i = 0; i < regions.size(); i++)
        log("  region %-2zu guest %s-%s %s", i + 1, hex(regions[i].start - base).c_str(),
            hex(regions[i].start + regions[i].size - base).c_str(), regions[i].readable ? "readable" : "-");

    // Windows code reaches "region 8" by summing the first 7 regions; that must be guest 0x02000000.
    uint64_t region8 = Memory::findRegionBaseAddress(base, 8);
    check(region8 - base == 0x02000000, "region 8 starts at guest 0x02000000 (got " + hex(region8 - base) + ")");

    // DataTypes::LocationAccess
    std::vector<int> locationSig = { 0x03, -1, 0xB5, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x10, 0x54, 0xF6, 0x44, 0x10, 0x2B, 0x7E, 0x78, 0x00, 0x00, 0x00 };
    uint64_t location = scanUntilFound(locationSig, 8, 0xe000000, false, "location");
    uint64_t locationGuest = location - base;
    log("Location: guest %s, map '%s', section '%s'", hex(locationGuest).c_str(),
        Memory::extractLocName(location + 0x14).c_str(), Memory::extractLocName(location + 0x40).c_str());

    uint32_t selfPointer = static_cast<uint32_t>(Memory::read_bigEndian4Bytes(location + 0x8, __FUNCTION__));
    check(selfPointer == locationGuest + 0x14, "read_bigEndian4Bytes: location self-pointer " + hex(selfPointer) + " == " + hex(locationGuest + 0x14));

    uint32_t viaOffset = static_cast<uint32_t>(Memory::read_bigEndian4BytesOffset(locationGuest + 0x8, __FUNCTION__));
    check(viaOffset == selfPointer, "read_bigEndian4BytesOffset agrees with read_bigEndian4Bytes");

    std::vector<BYTE> original = Memory::read_bytes(location + 0x14, 1, __FUNCTION__);
    Memory::write_byte(location + 0x14, original[0], __FUNCTION__);
    check(Memory::read_bytes(location + 0x14, 1, __FUNCTION__)[0] == original[0], "write_byte (wrote back the same byte)");

    // LocalInstance flag scans: region 8, offset 0.
    struct Flag { const char* name; std::vector<int> sig; };
    std::vector<Flag> flags = {
        { "Jugador10_DispNameFlag", { 0x82, 0xCB, 0xCC, 0x60, 0x00, -1, 0x00, 0x00, 0x10, 0x29, 0x84, 0x10 } },
        { "Jugador10_Status", { 0x87, 0x68, 0xFA, 0x81, 0x00, -1, 0x00, 0x00, 0x10, 0x29, 0x84, 0xC8 } },
        { "Jugador10_Name", { 0xF2, 0xA5, 0xD4, 0xA2, 0x00, -1, 0x00, 0x00, 0x10, 0x29, 0x86, 0x38 } },
        { "Jugador1_Hold", { 0x0D, 0x4A, 0x75, 0x67, 0x61, 0x64, 0x6F, -1, 0x31, 0x5F, 0x48, 0x6F, 0x6C, 0x64 } },
    };
    for (const Flag& flag : flags)
    {
        uint64_t addr = Memory::PatternScan(flag.sig, base, 8, 0);
        check(addr >= 30000, std::string("region 8 scan: ") + flag.name + (addr >= 30000 ? " at guest " + hex(addr - base) : " not found"));
    }

    // Quests_class::findBoolFlagToChange: region 0 across all following regions.
    std::vector<int> boolFlagSig = { 0x47, 0x62, 0x6F, 0x6F, 0x6C, 0x65, 0x61, -1, 0x46, 0x6C, 0x61, 0x67, 0x54, 0x6F, 0x43, 0x68, 0x61, 0x6E, 0x67, 0x65 };
    auto started = std::chrono::steady_clock::now();
    uint64_t boolFlag = Memory::PatternScan(boolFlagSig, base, 0, 0, false, true);
    long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    check(boolFlag >= 30000, "multi-region scan: BoolFlagToChange " + (boolFlag >= 30000 ? "at guest " + hex(boolFlag - base) : std::string("not found")) + " in " + std::to_string(elapsedMs) + " ms");

    log("%d check(s) failed", g_failures);

    log("Watching map/section names. Walk around in game: they should change.");
    std::string last;
    for (;;)
    {
        std::string current = Memory::extractLocName(location + 0x14) + " / " + Memory::extractLocName(location + 0x40);
        if (current != last)
        {
            log("%s", current.c_str());
            last = current;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

__attribute__((constructor)) void onLoad()
{
    if (!isCemuProcess())
        return;

    std::string logPath = getenv("BOTWM_MEMTEST_LOG") ? getenv("BOTWM_MEMTEST_LOG") : "/tmp/botwm-memtest-" + std::to_string(getpid()) + ".log";
    g_log = fopen(logPath.c_str(), "w");
    log("Logging to %s", logPath.c_str());

    std::thread(run).detach();
}

}
