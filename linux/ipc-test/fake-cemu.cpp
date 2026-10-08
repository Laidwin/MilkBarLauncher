// Stands in for Cemu 2.0-48 on Linux: no exports, but a non-PIE executable whose .symtab has
// memory_base, s_osFunctionTable and osLib_addFunctionInternal, which the mod looks up.
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct PPCInterpreter_t;
struct osFunctionEntry_t { std::string name; void* function; };

__attribute__((used)) std::vector<osFunctionEntry_t>* s_osFunctionTable;
__attribute__((used)) uint8_t* memory_base = nullptr;

__attribute__((used, noinline)) void osLib_addFunctionInternal(const char* libraryName, const char* functionName, void (*osFunction)(PPCInterpreter_t*))
{
    if (!s_osFunctionTable)
        s_osFunctionTable = new std::vector<osFunctionEntry_t>();
    s_osFunctionTable->push_back({ std::string(libraryName) + "." + functionName, (void*)osFunction });
    if (std::string(libraryName) != "coreinit")
        printf("[fake cemu] registered %s.%s\n", libraryName, functionName), fflush(stdout);
}

int main()
{
    // Like CafeSystem::Initialize: reserve guest memory, then register Cemu's own HLE functions.
    usleep(300000);
    memory_base = (uint8_t*)mmap(nullptr, 0x100000000ULL, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    for (int i = 0; i < 200; i++)
    {
        osLib_addFunctionInternal("coreinit", ("f" + std::to_string(i)).c_str(), nullptr);
        usleep(2000);
    }
    printf("[fake cemu] initialized\n"), fflush(stdout);
    sleep(20);
}
