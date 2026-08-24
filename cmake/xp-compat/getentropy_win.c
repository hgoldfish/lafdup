/* XP-compatible getentropy: LibreSSL's default uses BCryptGenRandom (Vista+).
 * RtlGenRandom (SystemFunction036 in advapi32) is available since Windows XP.
 */

#include <windows.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

int getentropy(void *buf, size_t len);

typedef BOOLEAN(WINAPI *RtlGenRandomFn)(PVOID, ULONG);

int getentropy(void *buf, size_t len)
{
    static RtlGenRandomFn rtlGenRandom = NULL;
    static int resolved = 0;

    if (len > 256) {
        errno = EIO;
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    if (!buf) {
        errno = EFAULT;
        return -1;
    }

    if (!resolved) {
        HMODULE advapi = LoadLibraryA("advapi32.dll");
        if (advapi) {
            rtlGenRandom = (RtlGenRandomFn)GetProcAddress(advapi, "SystemFunction036");
        }
        resolved = 1;
    }

    if (!rtlGenRandom || !rtlGenRandom(buf, (ULONG)len)) {
        errno = EIO;
        return -1;
    }
    return 0;
}
