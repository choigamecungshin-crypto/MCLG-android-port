#pragma once

// Minimal Win32 type and synchronization compatibility for the shared
// Legacy Console Edition sources when compiled by Android NDK/Clang.
// This is deliberately limited to declarations used by the portable headers;
// platform services still need real Android implementations.
#if defined(__ANDROID__) && !defined(_WIN32)
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <cstring>
#include <ctime>
#include <chrono>
#include <thread>
#include <pthread.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

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
using __uint64 = unsigned long long;
#ifndef CONST
#define CONST const
#endif
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using UINT = std::uint32_t;
using ULONG = std::uint32_t;
using LONG = std::int32_t;
using BOOL = int;
using FLOAT = float;
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
using PVOID = void*;
using LPCVOID = const void*;
using HANDLE = void*;
using PBYTE = BYTE*;
using LPBYTE = BYTE*;
using PDWORD = DWORD*;
using HRESULT = std::int32_t;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
struct LARGE_INTEGER { LONGLONG QuadPart; };

#ifndef ZeroMemory
#define ZeroMemory(destination, length) std::memset((destination), 0, (length))
#endif

static inline BOOL QueryPerformanceCounter(LARGE_INTEGER* value) {
    if (!value) return FALSE;
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    value->QuadPart = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    return TRUE;
}
static inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* value) {
    if (!value) return FALSE;
    value->QuadPart = 1000000000LL;
    return TRUE;
}
static inline void Sleep(DWORD milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// D3D11 handles are kept opaque at the legacy API boundary. The Android
// renderer implements the C4JRender contract on GLES and never dereferences
// these Direct3D objects.
struct ID3D11Device;
struct IDXGISwapChain;
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct D3D11_RECT { LONG left, top, right, bottom; };

enum {
    D3D11_BLEND_ZERO = 1,
    D3D11_BLEND_ONE = 2,
    D3D11_BLEND_SRC_COLOR = 3,
    D3D11_BLEND_INV_SRC_COLOR = 4,
    D3D11_BLEND_SRC_ALPHA = 5,
    D3D11_BLEND_INV_SRC_ALPHA = 6,
    D3D11_BLEND_DEST_ALPHA = 7,
    D3D11_BLEND_INV_DEST_ALPHA = 8,
    D3D11_BLEND_DEST_COLOR = 9,
    D3D11_BLEND_INV_DEST_COLOR = 10,
    D3D11_BLEND_BLEND_FACTOR = 14,
    D3D11_BLEND_INV_BLEND_FACTOR = 15,
    D3D11_COMPARISON_NEVER = 1,
    D3D11_COMPARISON_LESS = 2,
    D3D11_COMPARISON_EQUAL = 3,
    D3D11_COMPARISON_LESS_EQUAL = 4,
    D3D11_COMPARISON_GREATER = 5,
    D3D11_COMPARISON_NOT_EQUAL = 6,
    D3D11_COMPARISON_GREATER_EQUAL = 7,
    D3D11_COMPARISON_ALWAYS = 8
};

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
