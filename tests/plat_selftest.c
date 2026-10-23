/* Self-test of the Linux platform layer (runtime/plat.h, plat_posix.c, posix/win32_absent.h):
 *   clang -D_GNU_SOURCE -Iruntime/posix -Iruntime tests/plat_selftest.c runtime/plat_posix.c -lcrypto -o /tmp/plat_selftest && /tmp/plat_selftest */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <windows.h>

static volatile LONG g_count;
static DWORD WINAPI bump(LPVOID arg) { for (int i = 0; i < 100000; i++) InterlockedIncrement(&g_count); return (DWORD)(uintptr_t)arg; }
static HANDLE g_ev;
static DWORD WINAPI set_later(LPVOID arg) { (void)arg; Sleep(30); SetEvent(g_ev); return 0; }
static CRITICAL_SECTION g_cs;
static CONDITION_VARIABLE g_cv = CONDITION_VARIABLE_INIT;
static int g_flag;
static DWORD WINAPI signal_cv(LPVOID arg) { (void)arg; Sleep(10); EnterCriticalSection(&g_cs); g_flag = 1; WakeAllConditionVariable(&g_cv); LeaveCriticalSection(&g_cs); return 0; }
static LPVOID g_main_fiber, g_other;
static int g_fiber_steps;
static void fiber_fn(LPVOID arg) { g_fiber_steps += (int)(uintptr_t)arg; SwitchToFiber(g_main_fiber); g_fiber_steps *= 10; SwitchToFiber(g_main_fiber); }
static int g_once_runs;
static BOOL CALLBACK once_fn(PINIT_ONCE o, PVOID p, PVOID *c) { (void)o; (void)p; (void)c; g_once_runs++; return TRUE; }

int main(void) {
    /* atomics */
    volatile LONG64 a = 5;
    assert(InterlockedIncrement64(&a) == 6 && InterlockedExchangeAdd64(&a, 4) == 6 && a == 10);
    assert(InterlockedExchange64(&a, 1) == 10 && InterlockedCompareExchange64(&a, 7, 1) == 1 && a == 7);
    assert(InterlockedCompareExchange64(&a, 9, 1) == 7 && a == 7);

    /* threads: exit code, wait, suspended start */
    HANDLE t[4];
    for (int i = 0; i < 4; i++) t[i] = CreateThread(NULL, 0, bump, (LPVOID)(uintptr_t)(40 + i), i == 3 ? CREATE_SUSPENDED : 0, NULL);
    assert(WaitForSingleObject(t[3], 50) == WAIT_TIMEOUT);
    assert(ResumeThread(t[3]) == 1);
    assert(WaitForMultipleObjects(4, t, TRUE, 5000) == WAIT_OBJECT_0 && g_count == 400000);
    DWORD code;
    assert(GetExitCodeThread(t[2], &code) && code == 42 && GetThreadId(t[2]) != 0);
    for (int i = 0; i < 4; i++) CloseHandle(t[i]);

    /* events (auto and manual), semaphores, timers */
    g_ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    CloseHandle(CreateThread(NULL, 0, set_later, NULL, 0, NULL));
    assert(WaitForSingleObject(g_ev, 0) == WAIT_TIMEOUT && WaitForSingleObject(g_ev, 2000) == WAIT_OBJECT_0);
    assert(WaitForSingleObject(g_ev, 0) == WAIT_TIMEOUT);          /* auto-reset: taken */
    HANDLE m = CreateEventA(NULL, TRUE, TRUE, NULL);
    assert(WaitForSingleObject(m, 0) == WAIT_OBJECT_0 && WaitForSingleObject(m, 0) == WAIT_OBJECT_0);
    HANDLE sem = CreateSemaphoreA(NULL, 0, 2, NULL);
    assert(ReleaseSemaphore(sem, 2, NULL) && !ReleaseSemaphore(sem, 1, NULL));
    assert(WaitForSingleObject(sem, 0) == WAIT_OBJECT_0 && WaitForSingleObject(sem, 0) == WAIT_OBJECT_0 && WaitForSingleObject(sem, 0) == WAIT_TIMEOUT);
    HANDLE tm = CreateWaitableTimerExW(NULL, NULL, 2, TIMER_ALL_ACCESS);
    LARGE_INTEGER due = { .QuadPart = -200000 };                   /* 20 ms */
    LARGE_INTEGER q0, q1, f;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&q0);
    assert(SetWaitableTimer(tm, &due, 0, NULL, NULL, FALSE) && WaitForSingleObject(tm, 1000) == WAIT_OBJECT_0);
    QueryPerformanceCounter(&q1);
    int64_t ms = (q1.QuadPart - q0.QuadPart) * 1000 / f.QuadPart;
    assert(ms >= 19 && ms < 500);

    /* critical section (recursive) + condition variable, once */
    InitializeCriticalSection(&g_cs);
    EnterCriticalSection(&g_cs); assert(TryEnterCriticalSection(&g_cs)); LeaveCriticalSection(&g_cs);
    assert(!SleepConditionVariableCS(&g_cv, &g_cs, 1));             /* times out */
    CloseHandle(CreateThread(NULL, 0, signal_cv, NULL, 0, NULL));
    while (!g_flag) assert(SleepConditionVariableCS(&g_cv, &g_cs, 2000) || g_flag);
    LeaveCriticalSection(&g_cs);
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce(&once, once_fn, NULL, NULL); InitOnceExecuteOnce(&once, once_fn, NULL, NULL);
    assert(g_once_runs == 1);

    /* fibers */
    g_main_fiber = ConvertThreadToFiber(NULL);
    g_other = CreateFiberEx(1 << 16, 1 << 20, 0, fiber_fn, (LPVOID)3);
    SwitchToFiber(g_other); assert(g_fiber_steps == 3);
    SwitchToFiber(g_other); assert(g_fiber_steps == 30);
    DeleteFiber(g_other);

    /* memory: fixed address, reserve / commit / decommit / release, query, protect */
    uint8_t *g = plat_map_fixed(0x7100000000ull, 1 << 20);
    assert(g == (uint8_t *)0x7100000000ull && !plat_map_fixed(0x7100000000ull, 1 << 20));
    uint8_t *r = VirtualAlloc(NULL, 0x40000, MEM_RESERVE, PAGE_NOACCESS);
    MEMORY_BASIC_INFORMATION mi;
    assert(r && VirtualQuery(r, &mi, sizeof mi) && mi.State == MEM_RESERVE);
    assert(VirtualAlloc(r + 0x10000, 0x10000, MEM_COMMIT, PAGE_READWRITE) == r + 0x10000);
    r[0x10000] = 7;
    assert(VirtualQuery(r + 0x10000, &mi, sizeof mi) && mi.State == MEM_COMMIT && mi.Protect == PAGE_READWRITE);
    assert(!IsBadReadPtr(r + 0x10000, 16) && IsBadReadPtr(r, 16));
    assert(VirtualFree(r + 0x10000, 0x10000, MEM_DECOMMIT) && VirtualAlloc(r + 0x10000, 0x1000, MEM_COMMIT, PAGE_READWRITE) && r[0x10000] == 0);
    DWORD old;
    assert(VirtualProtect(r + 0x10000, 0x1000, PAGE_READONLY, &old) && old == PAGE_READWRITE);
    assert(!VirtualFree(r + 0x1000, 0, MEM_RELEASE) && VirtualFree(r, 0, MEM_RELEASE));
    assert(VirtualQuery(r, &mi, sizeof mi) && mi.State == MEM_FREE);

    /* write watch: only the pages written since the last reset, also when the kernel writes them (read() into the page) */
    {
        uint8_t *w = VirtualAlloc(NULL, 16 * 4096, MEM_RESERVE | MEM_WRITE_WATCH, PAGE_NOACCESS);
        PVOID pg[16];
        ULONG_PTR cnt = 16;
        ULONG gran;
        assert(w && VirtualAlloc(w, 16 * 4096, MEM_COMMIT, PAGE_READWRITE) == w);
        assert(GetWriteWatch(0, w, 16 * 4096, pg, &cnt, &gran) == 0 && cnt == 0 && gran == 4096);
        w[2 * 4096 + 7] = 1;
        int fds[2];
        assert(pipe(fds) == 0 && write(fds[1], "abc", 3) == 3 && read(fds[0], w + 5 * 4096, 3) == 3);
        (void)w[9 * 4096];                                             /* a read is not a write */
        cnt = 16;
        assert(GetWriteWatch(WRITE_WATCH_FLAG_RESET, w, 16 * 4096, pg, &cnt, &gran) == 0);
        int tracked = cnt == 2;                                       /* 16 = untracked fallback (every page) */
        if (tracked) assert(pg[0] == w + 2 * 4096 && pg[1] == w + 5 * 4096);
        else assert(cnt == 16);
        cnt = 16;
        assert(GetWriteWatch(0, w, 16 * 4096, pg, &cnt, &gran) == 0 && cnt == (tracked ? 0 : 16));
        w[3 * 4096] = 1; w[4 * 4096] = 1;
        cnt = 1;                                                       /* a short list: the first written page only */
        assert(GetWriteWatch(0, w, 16 * 4096, pg, &cnt, &gran) == 0 && cnt == 1 && (!tracked || pg[0] == w + 3 * 4096));
        assert(ResetWriteWatch(w, 16 * 4096) == 0);
        cnt = 16;
        assert(GetWriteWatch(0, w, 16 * 4096, pg, &cnt, &gran) == 0 && cnt == (tracked ? 0 : 16));
        assert(VirtualFree(w + 4096, 4096, MEM_DECOMMIT) && VirtualAlloc(w + 4096, 4096, MEM_COMMIT, PAGE_READWRITE) && w[4096] == 0);
        cnt = 16;
        assert(GetWriteWatch(0, w, 16 * 4096, pg, &cnt, &gran) == 0 && cnt == (tracked ? 0 : 16));   /* committed again: not written */
        uint8_t *plain = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        cnt = 16;
        assert(GetWriteWatch(0, plain, 4096, pg, &cnt, &gran) != 0);  /* not watched: fails */
        assert(VirtualFree(w, 0, MEM_RELEASE) && VirtualFree(plain, 0, MEM_RELEASE));
        printf("write watch: %s\n", tracked ? "tracked" : "NOT tracked (fallback)");
    }

    /* files: create, write at an offset, size, truncate, find, case-insensitive paths, attributes */
    char dir[64];
    snprintf(dir, sizeof dir, "/tmp/plat_selftest_%u", (unsigned)GetCurrentProcessId());
    assert(CreateDirectoryA(dir, NULL) && !CreateDirectoryA(dir, NULL));
    char path[128], back[128];
    snprintf(path, sizeof path, "%s\\Sub", dir); assert(CreateDirectoryA(path, NULL));
    snprintf(path, sizeof path, "%s\\Sub\\Data.BIN", dir);
    HANDLE fh = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert(fh != INVALID_HANDLE_VALUE);
    DWORD n;
    OVERLAPPED ov = { .Offset = 100 };
    assert(WriteFile(fh, "hello", 5, &n, &ov) && n == 5);
    LARGE_INTEGER sz;
    assert(GetFileSizeEx(fh, &sz) && sz.QuadPart == 105);
    LARGE_INTEGER to = { .QuadPart = 102 };
    assert(SetFilePointerEx(fh, to, NULL, FILE_BEGIN) && SetEndOfFile(fh) && GetFileSizeEx(fh, &sz) && sz.QuadPart == 102);
    char buf[8] = {0};
    ov.Offset = 100;
    assert(ReadFile(fh, buf, 8, &n, &ov) && n == 2 && !memcmp(buf, "he", 2));
    CloseHandle(fh);
    snprintf(back, sizeof back, "%s/SUB/data.bin", dir);              /* another case */
    FILE *fp = fopen(back, "rb");
    assert(fp && fgetc(fp) == 0);
    fclose(fp);
    snprintf(path, sizeof path, "%s\\sub\\*.bin", dir);
    WIN32_FIND_DATAA fd;
    HANDLE fnd = FindFirstFileA(path, &fd);
    assert(fnd != INVALID_HANDLE_VALUE && !strcmp(fd.cFileName, "Data.BIN") && fd.nFileSizeLow == 102 && !FindNextFileA(fnd, &fd));
    FindClose(fnd);
    WIN32_FILE_ATTRIBUTE_DATA fa;
    assert(GetFileAttributesExA(back, GetFileExInfoStandard, &fa) && fa.nFileSizeLow == 102);
    snprintf(path, sizeof path, "%s/Sub/copy.bin", dir);
    assert(CopyFileA(back, path, TRUE) && !CopyFileA(back, path, TRUE));
    assert(DeleteFileA(path) && DeleteFileA(back));
    snprintf(path, sizeof path, "%s/Sub", dir); assert(RemoveDirectoryA(path) && RemoveDirectoryA(dir));
    assert(GetFileAttributesA(dir) == INVALID_FILE_ATTRIBUTES);

    /* text and hashing */
    WCHAR w[16];
    char u[16];
    assert(MultiByteToWideChar(CP_UTF8, 0, "h\xc3\xa9", -1, w, 16) == 3 && w[1] == 0xe9);
    assert(WideCharToMultiByte(CP_UTF8, 0, w, -1, u, 16, NULL, NULL) == 4 && !strcmp(u, "h\xc3\xa9"));
    HCRYPTPROV prov; HCRYPTHASH h;
    BYTE dg[20]; DWORD dn = 20;
    assert(CryptAcquireContextA(&prov, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) && CryptCreateHash(prov, CALG_SHA1, 0, 0, &h));
    assert(CryptHashData(h, (const BYTE *)"abc", 3, 0) && CryptGetHashParam(h, HP_HASHVAL, dg, &dn, 0) && dn == 20);
    assert(dg[0] == 0xa9 && dg[1] == 0x99 && dg[19] == 0x9d);       /* SHA-1("abc") = a9993e36...9cd0d89d */
    CryptDestroyHash(h);

    puts("plat self-test: ok");
    return 0;
}
