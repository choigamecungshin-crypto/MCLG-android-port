#pragma once

// Minimal Win32 type and synchronization compatibility for the shared
// Legacy Console Edition sources when compiled by Android NDK/Clang.
// This is deliberately limited to declarations used by the portable headers;
// platform services still need real Android implementations.
#if defined(__ANDROID__) && !defined(_WIN32)
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <pthread.h>

#ifndef WINAPI
#define WINAPI
#endif
#ifndef CALLBACK
#define CALLBACK
#endif

using BYTE = std::uint8_t;
using VOID = void;
using __int64 = long long;
using __int32 = int;
#ifndef CONST
#define CONST const
#endif
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using UINT = std::uint32_t;
using ULONG = std::uint32_t;
using LONG = std::int32_t;
using BOOL = int;
using INT = int;
using SHORT = std::int16_t;
using USHORT = std::uint16_t;
using ULONGLONG = std::uint64_t;
using LONGLONG = std::int64_t;
using ULONG_PTR = std::uintptr_t;
using SIZE_T = std::size_t;
using WCHAR = wchar_t;
using CHAR = char;
using LPCWSTR = const wchar_t*;
using LPWSTR = wchar_t*;
using LPCSTR = const char*;
using LPSTR = char*;
using LPVOID = void*;
using LPCVOID = const void*;
using HANDLE = void*;
using PBYTE = BYTE*;
using LPBYTE = BYTE*;
using PDWORD = DWORD*;
using HRESULT = std::int32_t;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

struct FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; };
struct SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay;
    WORD wHour, wMinute, wSecond, wMilliseconds;
};

using CRITICAL_SECTION = pthread_mutex_t;

static inline BOOL InitializeCriticalSectionAndSpinCount(
    CRITICAL_SECTION* section, DWORD
) {
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) return FALSE;
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    const int result = pthread_mutex_init(section, &attr);
    pthread_mutexattr_destroy(&attr);
    return result == 0 ? TRUE : FALSE;
}
static inline void InitializeCriticalSection(CRITICAL_SECTION* section) {
    (void)InitializeCriticalSectionAndSpinCount(section, 0);
}
static inline void DeleteCriticalSection(CRITICAL_SECTION* section) {
    (void)pthread_mutex_destroy(section);
}
static inline void EnterCriticalSection(CRITICAL_SECTION* section) {
    (void)pthread_mutex_lock(section);
}
static inline void LeaveCriticalSection(CRITICAL_SECTION* section) {
    (void)pthread_mutex_unlock(section);
}
static inline BOOL TryEnterCriticalSection(CRITICAL_SECTION* section) {
    return pthread_mutex_trylock(section) == 0 ? TRUE : FALSE;
}
#endif
