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
#include <atomic>
#include <unordered_map>
#include <pthread.h>
#include <cerrno>
#include <sys/mman.h>
#include <unistd.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef __debugbreak
#define __debugbreak() __builtin_trap()
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
using LPDWORD = DWORD*;
using PLONG = LONG*;
using HRESULT = std::int32_t;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
struct LARGE_INTEGER { LONGLONG QuadPart; };

using PULONG_PTR = ULONG_PTR*;
static constexpr ULONG_PTR MAXULONG_PTR = static_cast<ULONG_PTR>(~static_cast<ULONG_PTR>(0));

#ifndef PAGE_READWRITE
#define PAGE_READWRITE 0x04
#endif
#ifndef MEM_COMMIT
#define MEM_COMMIT 0x1000
#endif
#ifndef MEM_RESERVE
#define MEM_RESERVE 0x2000
#endif
#ifndef MEM_DECOMMIT
#define MEM_DECOMMIT 0x4000
#endif
#ifndef MEM_RELEASE
#define MEM_RELEASE 0x8000
#endif
#ifndef FILE_BEGIN
#define FILE_BEGIN 0
#endif
#ifndef FILE_CURRENT
#define FILE_CURRENT 1
#endif
#ifndef FILE_END
#define FILE_END 2
#endif

struct MEMORYSTATUS {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    SIZE_T dwTotalPhys;
    SIZE_T dwAvailPhys;
    SIZE_T dwTotalPageFile;
    SIZE_T dwAvailPageFile;
    SIZE_T dwTotalVirtual;
    SIZE_T dwAvailVirtual;
    SIZE_T dwAvailExtendedVirtual;
};

static inline DWORD GetLastError() {
    return static_cast<DWORD>(errno);
}

static inline void GlobalMemoryStatus(MEMORYSTATUS* status) {
    if (!status) return;
    const long pageSize = sysconf(_SC_PAGESIZE);
    const long totalPages = sysconf(_SC_PHYS_PAGES);
    const long availablePages = sysconf(_SC_AVPHYS_PAGES);
    const SIZE_T total = pageSize > 0 && totalPages > 0
        ? static_cast<SIZE_T>(pageSize) * static_cast<SIZE_T>(totalPages) : 0;
    const SIZE_T available = pageSize > 0 && availablePages > 0
        ? static_cast<SIZE_T>(pageSize) * static_cast<SIZE_T>(availablePages) : 0;
    status->dwLength = sizeof(MEMORYSTATUS);
    status->dwMemoryLoad = total ? static_cast<DWORD>(((total - available) * 100) / total) : 0;
    status->dwTotalPhys = total;
    status->dwAvailPhys = available;
    status->dwTotalPageFile = total;
    status->dwAvailPageFile = available;
    status->dwTotalVirtual = static_cast<SIZE_T>(-1);
    status->dwAvailVirtual = static_cast<SIZE_T>(-1);
    status->dwAvailExtendedVirtual = 0;
}

// Implement the reserve/commit/decommit subset used by the Legacy save
// container. Reserved ranges are real Android virtual mappings; commits
// change page protection rather than allocating a second backing buffer.
static inline LPVOID VirtualAlloc(LPVOID address, SIZE_T size, DWORD type, DWORD) {
    if (size == 0) { errno = EINVAL; return nullptr; }
    if ((type & MEM_RESERVE) != 0) {
        if (address != nullptr) { errno = EINVAL; return nullptr; }
        void* result = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return result == MAP_FAILED ? nullptr : result;
    }
    if ((type & MEM_COMMIT) != 0) {
        if (address == nullptr || mprotect(address, size, PROT_READ | PROT_WRITE) != 0)
            return nullptr;
        return address;
    }
    errno = EINVAL;
    return nullptr;
}

static inline BOOL VirtualFree(LPVOID address, SIZE_T size, DWORD type) {
    if (address == nullptr) { errno = EINVAL; return FALSE; }
    if ((type & MEM_DECOMMIT) != 0) {
        if (size == 0 || mprotect(address, size, PROT_NONE) != 0) return FALSE;
        (void)madvise(address, size, MADV_DONTNEED);
        return TRUE;
    }
    if ((type & MEM_RELEASE) != 0) {
        return munmap(address, size) == 0 ? TRUE : FALSE;
    }
    errno = EINVAL;
    return FALSE;
}

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
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11Texture2D;
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
#ifndef THREAD_PRIORITY_HIGHEST
#define THREAD_PRIORITY_HIGHEST 2
#endif
#ifndef THREAD_PRIORITY_ABOVE_NORMAL
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#endif
#ifndef THREAD_PRIORITY_NORMAL
#define THREAD_PRIORITY_NORMAL 0
#endif
#ifndef THREAD_PRIORITY_BELOW_NORMAL
#define THREAD_PRIORITY_BELOW_NORMAL -1
#endif
#ifndef THREAD_PRIORITY_LOWEST
#define THREAD_PRIORITY_LOWEST -2
#endif
#ifndef INFINITE
#define INFINITE 0xFFFFFFFFu
#endif
#ifndef STILL_ACTIVE
#define STILL_ACTIVE 259u
#endif
#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0 0u
#endif
#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT 258u
#endif
#ifndef WAIT_ABANDONED
#define WAIT_ABANDONED 0x80u
#endif
#ifndef WAIT_FAILED
#define WAIT_FAILED 0xFFFFFFFFu
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

static inline std::unordered_map<DWORD, LPVOID>& AndroidTlsValues() {
    static thread_local std::unordered_map<DWORD, LPVOID> values;
    return values;
}
static inline DWORD TlsAlloc() {
    static std::atomic<DWORD> nextIndex{1};
    return nextIndex.fetch_add(1, std::memory_order_relaxed);
}
static inline BOOL TlsSetValue(DWORD index, LPVOID value) {
    AndroidTlsValues()[index] = value;
    return TRUE;
}
static inline LPVOID TlsGetValue(DWORD index) {
    auto& values = AndroidTlsValues();
    const auto found = values.find(index);
    return found == values.end() ? nullptr : found->second;
}
static inline BOOL TlsFree(DWORD index) {
    AndroidTlsValues().erase(index);
    return TRUE;
}
 
// POSIX-backed file/time compatibility used by the original optional save dump
// paths. Game saves themselves are handled by Android_Storage.cpp.
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef S_OK
#define S_OK ((HRESULT)0)
#endif
#ifndef GENERIC_WRITE
#define GENERIC_WRITE 0x40000000u
#endif
#ifndef OPEN_ALWAYS
#define OPEN_ALWAYS 4u
#endif
#ifndef FILE_FLAG_RANDOM_ACCESS
#define FILE_FLAG_RANDOM_ACCESS 0x10000000u
#endif
#ifndef swscanf_s
#define swscanf_s swscanf
#endif

static inline void GetSystemTime(SYSTEMTIME* output) {
    if (!output) return;
    const std::time_t now = std::time(nullptr);
    std::tm utc{};
    gmtime_r(&now, &utc);
    output->wYear = static_cast<WORD>(utc.tm_year + 1900);
    output->wMonth = static_cast<WORD>(utc.tm_mon + 1);
    output->wDayOfWeek = static_cast<WORD>(utc.tm_wday);
    output->wDay = static_cast<WORD>(utc.tm_mday);
    output->wHour = static_cast<WORD>(utc.tm_hour);
    output->wMinute = static_cast<WORD>(utc.tm_min);
    output->wSecond = static_cast<WORD>(utc.tm_sec);
    output->wMilliseconds = 0;
}

// Keep Win32 HANDLE opaque to engine code while mapping these debug-file
// operations to normal Android app-private POSIX file descriptors.
static inline HANDLE AndroidCreateFileCompat(LPCSTR path, DWORD, DWORD, LPVOID,
                                              DWORD creation, DWORD, HANDLE) {
    if (!path || !*path) return nullptr;
    int flags = O_WRONLY | O_CREAT;
    if (creation != OPEN_ALWAYS) flags |= O_TRUNC;
    const int fd = ::open(path, flags, 0660);
    if (fd < 0) return nullptr;
    return reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(fd) + 1);
}
static inline BOOL AndroidWriteFileCompat(HANDLE handle, LPCVOID buffer, DWORD bytes,
                                           LPDWORD written, LPVOID) {
    if (written) *written = 0;
    if (!handle || (!buffer && bytes != 0)) return FALSE;
    const int fd = static_cast<int>(reinterpret_cast<std::intptr_t>(handle) - 1);
    const ssize_t result = ::write(fd, buffer, bytes);
    if (result < 0) return FALSE;
    if (written) *written = static_cast<DWORD>(result);
    return static_cast<DWORD>(result) == bytes ? TRUE : FALSE;
}
static inline BOOL AndroidCloseHandleCompat(HANDLE handle) {
    if (!handle) return FALSE;
    const int fd = static_cast<int>(reinterpret_cast<std::intptr_t>(handle) - 1);
    return ::close(fd) == 0 ? TRUE : FALSE;
}
#ifndef CreateFile
#define CreateFile AndroidCreateFileCompat
#endif
#ifndef WriteFile
#define WriteFile AndroidWriteFileCompat
#endif
#ifndef CloseHandle
#define CloseHandle AndroidCloseHandleCompat
#endif

#endif
