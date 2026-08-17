#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <taihen.h>

#include "vitabtfix.h"
#include "log.h"

static SceUID g_thread = -1;
static SceUID g_ts_uid = -1;
static volatile int g_stop;
static int g_ready;
static int g_patch_tried;

static const unsigned char ts_inc_8000[4] = {
    TS_INC_8000_0, TS_INC_8000_1, TS_INC_8000_2, TS_INC_8000_3
};
static const unsigned char ts_inc_512[4] = {
    TS_INC_8000_0, TS_INC_8000_1, TS_INC_512_2, TS_INC_512_3
};

static int bytes4_eq(const unsigned char *p, const unsigned char *q)
{
    return p[0] == q[0] && p[1] == q[1] && p[2] == q[2] && p[3] == q[3];
}

static void install_ts_patch(void)
{
    tai_module_info_t tinfo;
    SceKernelModuleInfo minfo;
    const unsigned char *base;
    unsigned int memsz, off, found_8000, found_512, n8000, n512;
    int ret;

    if (g_patch_tried)
        return;
    g_patch_tried = 1;

    memset(&tinfo, 0, sizeof(tinfo));
    tinfo.size = sizeof(tinfo);
    ret = taiGetModuleInfoForKernel(KERNEL_PID, "SceBt", &tinfo);
    if (ret < 0) {
        log_printf("patch: SceBt not found 0x%08X", (unsigned int)ret);
        return;
    }

    memset(&minfo, 0, sizeof(minfo));
    minfo.size = sizeof(minfo);
    ret = ksceKernelGetModuleInfo(KERNEL_PID, tinfo.modid, &minfo);
    if (ret < 0) {
        log_printf("patch: GetModuleInfo 0x%08X", (unsigned int)ret);
        return;
    }

    n8000 = 0;
    n512 = 0;
    found_8000 = 0;
    found_512 = 0;

    base = (const unsigned char *)minfo.segments[0].vaddr;
    memsz = (unsigned int)minfo.segments[0].memsz;
    if (!base || memsz < 4) {
        log_printf("patch: SceBt seg0 unreadable");
        return;
    }
    for (off = 0; off + 4 <= memsz; off += 2) {
        if (bytes4_eq(base + off, ts_inc_8000)) {
            n8000++;
            if (n8000 == 1)
                found_8000 = off;
        } else if (bytes4_eq(base + off, ts_inc_512)) {
            n512++;
            if (n512 == 1)
                found_512 = off;
        }
    }

    if (n8000 == 1) {
        g_ts_uid = taiInjectDataForKernel(KERNEL_PID, tinfo.modid, 0,
                                         found_8000, ts_inc_512, 4);
        log_printf("patch: +%X -> +512 %s", found_8000,
                   g_ts_uid > 0 ? "ok" : "FAILED");
        return;
    }
    if (n8000 == 0 && n512 == 1) {
        log_printf("patch: already +512 at +%X", found_512);
        return;
    }
    if (n8000 > 1) {
        log_printf("patch: %u copies of +8000 add, not touching", n8000);
        return;
    }
    log_printf("patch: no +8000 timestamp add in SceBt (other firmware encoding?)");
}

static int try_mount_ready(void)
{
    if (g_ready)
        return 1;
    if (log_init() < 0)
        return 0;
    g_ready = 1;
    install_ts_patch();
    log_printf("vitabtfix.skprx %s ready patch=%s",
               PLUGIN_VERSION, g_ts_uid > 0 ? "on" : "off");
    return 1;
}

static int worker_thread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    while (!g_stop) {
        try_mount_ready();
        ksceKernelDelayThread(200 * 1000);
    }
    return 0;
}

int module_start(SceSize argc, const void *args)
{
    (void)argc;
    (void)args;

    g_thread = ksceKernelCreateThread("vitabtfix_worker", worker_thread, 0x3C, 0x2000, 0, 0x10000, 0);
    if (g_thread >= 0)
        ksceKernelStartThread(g_thread, 0, 0);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
    (void)argc;
    (void)args;

    g_stop = 1;
    if (g_thread >= 0) {
        ksceKernelWaitThreadEnd(g_thread, 0, 0);
        ksceKernelDeleteThread(g_thread);
        g_thread = -1;
    }
    if (g_ts_uid > 0) {
        taiInjectReleaseForKernel(g_ts_uid);
        g_ts_uid = -1;
    }
    log_shutdown();
    return SCE_KERNEL_STOP_SUCCESS;
}

void _start(void)
{
    module_start(0, 0);
}
