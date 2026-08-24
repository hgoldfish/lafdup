/* Shims for APIs missing on Windows XP but still referenced by MinGW/LibreSSL. */

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string.h>

/* ---- inet_pton / inet_ntop (Vista+ in ws2_32; provide locals + import stubs) ---- */

static int xp_parse_ipv4_octet(const char **pp, unsigned int *out)
{
    const char *p = *pp;
    unsigned int v = 0;
    int digits = 0;
    if (*p < '0' || *p > '9')
        return 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10u + (unsigned int)(*p - '0');
        if (v > 255)
            return 0;
        ++p;
        ++digits;
        if (digits > 3)
            return 0;
    }
    *out = v;
    *pp = p;
    return 1;
}

static int xp_inet_pton4(const char *src, void *dst)
{
    const char *p = src;
    unsigned int o[4];
    int i;
    for (i = 0; i < 4; ++i) {
        if (!xp_parse_ipv4_octet(&p, &o[i]))
            return 0;
        if (i < 3) {
            if (*p != '.')
                return 0;
            ++p;
        }
    }
    if (*p != '\0')
        return 0;
    {
        unsigned char *out = (unsigned char *)dst;
        out[0] = (unsigned char)o[0];
        out[1] = (unsigned char)o[1];
        out[2] = (unsigned char)o[2];
        out[3] = (unsigned char)o[3];
    }
    return 1;
}

static int xp_inet_pton6(const char *src, void *dst)
{
    struct sockaddr_in6 sa;
    int size = sizeof(sa);
    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    if (WSAStringToAddressA((LPSTR)src, AF_INET6, NULL, (LPSOCKADDR)&sa, &size) != 0)
        return 0;
    memcpy(dst, &sa.sin6_addr, sizeof(sa.sin6_addr));
    return 1;
}

int WSAAPI inet_pton(int af, const char *src, void *dst)
{
    if (!src || !dst)
        return -1;
    if (af == AF_INET)
        return xp_inet_pton4(src, dst);
    if (af == AF_INET6)
        return xp_inet_pton6(src, dst);
    return -1;
}

static const char *xp_inet_ntop4(const void *src, char *dst, size_t size)
{
    const unsigned char *a = (const unsigned char *)src;
    char tmp[16];
    int n = wsprintfA(tmp, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    if (n <= 0 || (size_t)n >= size)
        return NULL;
    memcpy(dst, tmp, (size_t)n + 1);
    return dst;
}

static const char *xp_inet_ntop6(const void *src, char *dst, size_t size)
{
    struct sockaddr_in6 sa;
    DWORD len = (DWORD)size;
    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    memcpy(&sa.sin6_addr, src, sizeof(sa.sin6_addr));
    if (WSAAddressToStringA((LPSOCKADDR)&sa, sizeof(sa), NULL, dst, &len) != 0)
        return NULL;
    return dst;
}

const char *WSAAPI inet_ntop(int af, const void *src, char *dst, size_t size)
{
    if (!src || !dst || size == 0)
        return NULL;
    if (af == AF_INET)
        return xp_inet_ntop4(src, dst, size);
    if (af == AF_INET6)
        return xp_inet_ntop6(src, dst, size);
    return NULL;
}

/* ---- GetTickCount64 (Vista+; libwinpthread may reference it) ---- */

ULONGLONG WINAPI GetTickCount64(void)
{
    return (ULONGLONG)GetTickCount();
}

/* ---- Vectored Exception Handling (Vista+; libwinpthread uses it for thread
       cancellation).  XP has no VEH, so registration simply fails. ---- */

#if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0600)
typedef LONG(WINAPI *XpVectoredHandler)(PEXCEPTION_POINTERS);

PVOID WINAPI AddVectoredExceptionHandler(ULONG First, XpVectoredHandler Handler)
{
    (void)First;
    (void)Handler;
    return NULL;
}

ULONG WINAPI RemoveVectoredExceptionHandler(PVOID Handle)
{
    (void)Handle;
    return 0;
}
#endif

#ifdef _WIN64
ULONGLONG(WINAPI *__imp_GetTickCount64)(void) = GetTickCount64;
int(WSAAPI *__imp_inet_pton)(int, const char *, void *) = inet_pton;
const char *(WSAAPI *__imp_inet_ntop)(int, const void *, char *, size_t) = inet_ntop;
#else
/* i386: msvcrt imports are cdecl; kernel32 GetTickCount64 is stdcall. */
asm(".section .data\n"
    ".globl __imp__GetTickCount64@0\n"
    "__imp__GetTickCount64@0:\n"
    "  .long _GetTickCount64@0\n"
    ".globl __imp__inet_pton@12\n"
    "__imp__inet_pton@12:\n"
    "  .long _inet_pton@12\n"
    ".globl __imp__inet_ntop@16\n"
    "__imp__inet_ntop@16:\n"
    "  .long _inet_ntop@16\n"
    ".globl __imp__AddVectoredExceptionHandler@8\n"
    "__imp__AddVectoredExceptionHandler@8:\n"
    "  .long _AddVectoredExceptionHandler@8\n"
    ".globl __imp__RemoveVectoredExceptionHandler@4\n"
    "__imp__RemoveVectoredExceptionHandler@4:\n"
    "  .long _RemoveVectoredExceptionHandler@4\n");
#endif
