#pragma once

// The subset of the Windows API used across the mod. On Windows this is just <Windows.h>;
// elsewhere it provides equivalents so shared code builds unchanged.

#ifdef _WIN32

#include <Windows.h>

#else

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <thread>

typedef uint32_t DWORD;
typedef uint8_t BYTE;
typedef uint32_t UINT32;
typedef uint8_t byte;
typedef int BOOL;
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* LPVOID;
typedef const wchar_t* LPCWSTR;
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);

#define TRUE 1
#define FALSE 0
#define MB_OK 0

// The mod never keeps thread handles, so threads are simply detached.
inline HANDLE CreateThread(void*, size_t, LPTHREAD_START_ROUTINE start, LPVOID parameter, DWORD, DWORD*)
{
    std::thread([start, parameter] { start(parameter); }).detach();
    return nullptr;
}

// No message boxes from inside Cemu on Linux; the text goes to Cemu's stderr.
inline int MessageBoxW(void*, LPCWSTR text, LPCWSTR, unsigned)
{
    fprintf(stderr, "[BOTWM] %ls\n", text);
    return 0;
}

inline DWORD GetTickCount()
{
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<DWORD>(now.tv_sec * 1000ull + now.tv_nsec / 1000000);
}

inline void Sleep(DWORD milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

template <size_t N>
int strcpy_s(char (&destination)[N], const char* source)
{
    snprintf(destination, N, "%s", source);
    return 0;
}

template <size_t N>
int strcat_s(char (&destination)[N], const char* source)
{
    size_t length = strnlen(destination, N);
    snprintf(destination + length, N - length, "%s", source);
    return 0;
}

inline int localtime_s(tm* result, const time_t* time)
{
    return localtime_r(time, result) ? 0 : 1;
}

#endif
