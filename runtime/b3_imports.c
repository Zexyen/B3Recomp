/* [b3recomp] Imports of Bayonetta 3's main (nnSdk 13.3.2) that B2recomp's runtime had no handler for, reached at boot.
 * Registered by boot.c after r2_imports.c (later registrations replace earlier ones). Each one says what it models;
 * the "no-op" ones are services whose answer cannot change what a PC build does (notifications, recording, mouse). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "runtime.h"
#include "hostrt.h"
#include "image_layout.h"

#define A0 (c->x[0])
#define A1 (c->x[1])
#define A2 (c->x[2])
#define RET(v) (c->x[0] = (uint64_t)(v))

static void nothing(Ctx *c) { (void)c; }
static void ret0(Ctx *c) { RET(0); }
static void ret2(Ctx *c) { RET(2); }

/* pthread_once(pthread_once_t *, void (*)(void)): musl's once word, 0 = not run, 1 = running, 2 = done.
 * shortcut: waiters spin on Sleep(0) (as cxx_call_once below); fine for one-time init, upgrade if a profile shows it */
static void b3_pthread_once(Ctx *c) {
    volatile LONG *once = (volatile LONG *)(uintptr_t)A0;
    uint64_t fn = A1;
    if (InterlockedCompareExchange(once, 1, 0) == 0) {
        call_guest(c, fn);
        InterlockedExchange(once, 2);
    } else {
        while (*once != 2) Sleep(0);
    }
    RET(0);
}

/* nn::os::SdkMutexType: one 32-bit word in guest memory, 0 free, 1 held. Not recursive (the SDK's isn't).
 * shortcut: spins with Sleep(0) instead of the kernel's ArbitrateLock wait queue; upgrade to a futex if it shows in a profile */
static void sdkmutex_init(Ctx *c) { wr32(A0, 0); }
static void sdkmutex_lock(Ctx *c) {
    volatile LONG *w = (volatile LONG *)(uintptr_t)A0;
    while (InterlockedCompareExchange(w, 1, 0) != 0) Sleep(0);
}
static void sdkmutex_unlock(Ctx *c) { InterlockedExchange((volatile LONG *)(uintptr_t)A0, 0); }

/* nn::os::ReaderWriterLockType: the guest object's first 8 bytes point at host state (made by Initialize, or on first use
 * of a zero-filled object). nn::os semantics: the writer may take the read and write locks again (counted), readers wait
 * while another thread writes, a writer waits for no readers and no other writer. Thread identity = the guest thread
 * pointer (one per nn::os thread). shortcut: no writer preference (a stream of readers can starve a writer); upgrade if
 * a hang shows a writer waiting. */
typedef struct { SRWLOCK m; CONDITION_VARIABLE cv; int readers, writes, owner_reads; uint64_t owner; } RwLock;
static RwLock *rw_get(uint64_t obj) {
    RwLock *r = (RwLock *)(uintptr_t)rd64(obj);
    if (r) return r;
    RwLock *n = calloc(1, sizeof *n);
    InitializeSRWLock(&n->m);
    InitializeConditionVariable(&n->cv);
    if (InterlockedCompareExchange64((volatile LONG64 *)(uintptr_t)obj, (LONG64)(uintptr_t)n, 0) != 0) { free(n); return (RwLock *)(uintptr_t)rd64(obj); }
    return n;
}
static void rw_init(Ctx *c) { wr64(A0, 0); rw_get(A0); }
static void rw_fini(Ctx *c) { RwLock *r = (RwLock *)(uintptr_t)rd64(A0); wr64(A0, 0); if (r) free(r); }
static void rw_read(Ctx *c) {
    RwLock *r = rw_get(A0);
    AcquireSRWLockExclusive(&r->m);
    if (r->writes && r->owner == c->tpidr) r->owner_reads++;
    else { while (r->writes) SleepConditionVariableSRW(&r->cv, &r->m, INFINITE, 0); r->readers++; }
    ReleaseSRWLockExclusive(&r->m);
}
static void rw_read_release(Ctx *c) {
    RwLock *r = rw_get(A0);
    AcquireSRWLockExclusive(&r->m);
    if (r->writes && r->owner == c->tpidr && r->owner_reads) r->owner_reads--;
    else if (r->readers) r->readers--;
    WakeAllConditionVariable(&r->cv);
    ReleaseSRWLockExclusive(&r->m);
}
static void rw_write(Ctx *c) {
    RwLock *r = rw_get(A0);
    AcquireSRWLockExclusive(&r->m);
    if (!(r->writes && r->owner == c->tpidr)) {
        while (r->writes || r->readers) SleepConditionVariableSRW(&r->cv, &r->m, INFINITE, 0);
        r->owner = c->tpidr;
    }
    r->writes++;
    ReleaseSRWLockExclusive(&r->m);
}
static void rw_write_release(Ctx *c) {
    RwLock *r = rw_get(A0);
    AcquireSRWLockExclusive(&r->m);
    if (r->writes && r->owner == c->tpidr && --r->writes == 0) { r->owner = 0; r->readers += r->owner_reads; r->owner_reads = 0; }
    WakeAllConditionVariable(&r->cv);
    ReleaseSRWLockExclusive(&r->m);
}

/* nn::util::pmr: NewDeleteResource() is a MemoryResource whose do_allocate / do_deallocate go to the game's own
 * aligned_alloc / free (main exports both, image_layout.h), as operator new / delete do in the SDK.
 * vtable (Itanium, as nn::MemoryResource): [0] ~dtor, [1] deleting dtor, [2] do_allocate(this, bytes, align),
 * [3] do_deallocate(this, p, bytes, align), [4] do_is_equal(this, other). */
static uint64_t g_nd_vtable[5];
static uint64_t g_nd_object[1];                      /* { vtable } */
static uint64_t g_default_resource;
static void nd_allocate(Ctx *c) {
    uint64_t bytes = A1, align = A2;
    c->x[0] = align < 16 ? 16 : align;
    c->x[1] = bytes ? bytes : 1;
    call_guest(c, GUEST_BASE + GAME_ALIGNED_ALLOC_OFF);
}
static void nd_deallocate(Ctx *c) { c->x[0] = A1; call_guest(c, GUEST_BASE + GAME_FREE_OFF); }
static void nd_is_equal(Ctx *c) { RET(A0 == A1); }
static uint64_t new_delete_resource(void) {
    if (!g_nd_vtable[0]) {
        g_nd_vtable[0] = hostfn_address("pmr::NewDeleteResource::~", nothing);
        g_nd_vtable[1] = hostfn_address("pmr::NewDeleteResource::~ (deleting)", nothing);
        g_nd_vtable[2] = hostfn_address("pmr::NewDeleteResource::do_allocate", nd_allocate);
        g_nd_vtable[3] = hostfn_address("pmr::NewDeleteResource::do_deallocate", nd_deallocate);
        g_nd_vtable[4] = hostfn_address("pmr::NewDeleteResource::do_is_equal", nd_is_equal);
        g_nd_object[0] = (uint64_t)(uintptr_t)g_nd_vtable;
    }
    return (uint64_t)(uintptr_t)g_nd_object;
}
static void pmr_NewDeleteResource(Ctx *c) { RET(new_delete_resource()); }
static void pmr_GetDefaultResource(Ctx *c) { RET(g_default_resource ? g_default_resource : new_delete_resource()); }
static void pmr_SetDefaultResource(Ctx *c) {         /* returns the previous one; null sets the new/delete resource back */
    uint64_t old = g_default_resource ? g_default_resource : new_delete_resource();
    g_default_resource = A0;
    RET(old);
}

/* nn::fs::Get/SetCurrentThreadFsContext: the context only decides whether an fs error aborts the program; the host
 * never aborts on one, so one shared context object is handed out and Set is remembered per thread.
 * shortcut: the context's contents are never read; model them if the game inspects what Get returns */
static uint64_t g_fs_context[4];
static _Thread_local uint64_t t_fs_context;
static void fs_GetCurrentThreadFsContext(Ctx *c) { RET(t_fs_context ? t_fs_context : (uint64_t)(uintptr_t)g_fs_context); }
static void fs_SetCurrentThreadFsContext(Ctx *c) { t_fs_context = A0; }

/* nn::oe::GetPerformanceMode: 1 (boost) docked, 0 (normal) handheld: the same answer as GetOperationMode. */
static void oe_GetPerformanceMode(Ctx *c) {
    ImportFn op = import_lookup("_ZN2nn2oe16GetOperationModeEv");
    if (op) op(c); else RET(1);
}

/* nn::hid::GetMouseState(MouseState *): no mouse on the console side (the PC mouse drives the camera through nnpad);
 * samplingNumber, x, y, deltas, wheel, buttons, attributes all 0 = not connected. */
static void hid_GetMouseState(Ctx *c) { memset((void *)(uintptr_t)A0, 0, 40); }

/* libc++ threads (Nintendo's libc++ on nn::os). __libcpp_thread_get_current_id: a value that is the same on every call
 * from one thread and differs between threads, never 0 (0 is "no thread"; protobuf's init-recursion check compares it). */
static void cxx_thread_get_current_id(Ctx *c) { RET((uint64_t)GetCurrentThreadId() + 1); }
/* std::__call_once(volatile unsigned long &flag, void *arg, void (*fn)(void *)): libc++'s flag, 0 not run, 1 running,
 * ~0 done. A throwing fn would leave 1 here (libc++ resets it to 0); B3's callers do not throw. */
static void cxx_call_once(Ctx *c) {
    volatile LONG64 *flag = (volatile LONG64 *)(uintptr_t)A0;
    uint64_t arg = A1, fn = A2;
    if (InterlockedCompareExchange64(flag, 1, 0) == 0) {
        c->x[0] = arg;
        call_guest(c, fn);
        InterlockedExchange64(flag, -1);
    } else {
        while (*flag != -1) Sleep(0);
    }
}
/* std::this_thread::sleep_for(const chrono::nanoseconds &) */
static void cxx_sleep_for(Ctx *c) {
    int64_t ns = (int64_t)rd64(A0);
    if (ns > 0) Sleep((DWORD)((ns + 999999) / 1000000));
    else Sleep(0);
}

/* __nnmusl_init_dso: main's DT_INIT (musl _init) hands nnmusl the module's layout, linker symbols in x0..x7 and on the
 * stack, all of them relocated (a plain number such as an alignment arrives as image base + number). Read from B3's
 * main (docs/RE_NOTES.md, "TLS"): x0 = image base, x2 = .tdata start, x3 = .tdata end, x4 = base + TLS alignment,
 * x7 = end of the TLS block (.tbss end). The block sits at thread pointer + 16 rounded up to the alignment (AArch64
 * TLS variant 1, 16-byte TCB): main reads tpidr_el0 + 16 .. + 0x68 with local-exec offsets. Answer 1: _init then
 * skips its own init_array loop (boot.c runs the constructors). */
static uint64_t g_tls_image, g_tls_image_size, g_tls_size, g_tls_off;
void tls_init_block(uint64_t tp) {                      /* os_threads.c: every new thread block */
    if (!g_tls_size || !tp) return;
    memcpy((void *)(uintptr_t)(tp + g_tls_off), (const void *)(uintptr_t)g_tls_image, g_tls_image_size);
    memset((void *)(uintptr_t)(tp + g_tls_off + g_tls_image_size), 0, g_tls_size - g_tls_image_size);
}
static void musl_init_dso(Ctx *c) {
    uint64_t align = c->x[4] - c->x[0];
    g_tls_image = c->x[2];
    g_tls_image_size = c->x[3] - c->x[2];
    g_tls_size = c->x[7] - c->x[2];
    g_tls_off = (16 + align - 1) & ~(align - 1);
    if (!align || align & (align - 1) || c->x[3] < c->x[2] || c->x[7] < c->x[3] || g_tls_off + g_tls_size > 0x1d8)
        guest_trap("__nnmusl_init_dso: unexpected TLS layout", c->x[2]);   /* below the thread block's errno / ThreadType */
    printf("nnmusl: main's TLS block: %llu bytes (%llu initialised) at thread pointer + %llu (main thread block %llx)\n", (unsigned long long)g_tls_size,
           (unsigned long long)g_tls_image_size, (unsigned long long)g_tls_off, (unsigned long long)c->tpidr);
    tls_init_block(c->tpidr);
    RET(1);
}

static const ImportDef b3_imports[] = {
    {"__nnmusl_init_dso", musl_init_dso},
    {"_ZNSt3__130__libcpp_thread_get_current_idEv", cxx_thread_get_current_id},
    {"_ZNSt3__111__call_onceERVmPvPFvS2_E", cxx_call_once},
    {"_ZNSt3__111this_thread9sleep_forERKNS_6chrono8durationIxNS_5ratioILl1ELl1000000000EEEEE", cxx_sleep_for},
    {"pthread_once", b3_pthread_once},
    {"_ZN2nn2os12SdkMutexType10InitializeEv", sdkmutex_init},
    {"_ZN2nn2os12SdkMutexType4LockEv", sdkmutex_lock},
    {"_ZN2nn2os12SdkMutexType6UnlockEv", sdkmutex_unlock},
    {"_ZN2nn2os26InitializeReaderWriterLockEPNS0_20ReaderWriterLockTypeE", rw_init},
    {"_ZN2nn2os24FinalizeReaderWriterLockEPNS0_20ReaderWriterLockTypeE", rw_fini},
    {"_ZN2nn2os15AcquireReadLockEPNS0_20ReaderWriterLockTypeE", rw_read},
    {"_ZN2nn2os15ReleaseReadLockEPNS0_20ReaderWriterLockTypeE", rw_read_release},
    {"_ZN2nn2os16AcquireWriteLockEPNS0_20ReaderWriterLockTypeE", rw_write},
    {"_ZN2nn2os16ReleaseWriteLockEPNS0_20ReaderWriterLockTypeE", rw_write_release},
    {"_ZN2nn4util3pmr17NewDeleteResourceEv", pmr_NewDeleteResource},
    {"_ZN2nn4util3pmr18GetDefaultResourceEv", pmr_GetDefaultResource},
    {"_ZN2nn4util3pmr18SetDefaultResourceEPNS_14MemoryResourceE", pmr_SetDefaultResource},
    {"_ZN2nn2fs25GetCurrentThreadFsContextEv", fs_GetCurrentThreadFsContext},
    {"_ZN2nn2fs25SetCurrentThreadFsContextEPKNS0_9FsContextE", fs_SetCurrentThreadFsContext},
    {"_ZN2nn2oe18GetPerformanceModeEv", oe_GetPerformanceMode},
    {"_ZN2nn2oe28SetResumeNotificationEnabledEb", nothing},
    {"_ZN2nn2oe44SetPerformanceModeChangedNotificationEnabledEb", nothing},
    {"_ZN2nn2oe38SetUserInactivityDetectionTimeExtendedEb", nothing},
    {"_ZN2nn2oe13MuteRecordingEv", nothing},
    {"_ZN2nn2oe15UnmuteRecordingEv", nothing},
    {"_ZN2nn3hid29SetNpadHandheldActivationModeENS0_26NpadHandheldActivationModeE", nothing},
    {"_ZN2nn3hid15InitializeMouseEv", nothing},
    {"_ZN2nn5audio41GetActiveAudioDeviceChannelCountForOutputEv", ret2},          /* the host output is opened as stereo */
    {"_ZN2nn3hid13GetMouseStateEPNS0_10MouseStateE", hid_GetMouseState},
    {"_ZN2nn3hid21ShowControllerSupportERKNS0_20ControllerSupportArgE", ret0},   /* the applet: "controllers are fine" */
};

void register_b3_imports(void) {
    register_imports(b3_imports, (int)(sizeof b3_imports / sizeof b3_imports[0]));
}
