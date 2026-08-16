#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/bt.h>
#include <taihen.h>

#include "vitabtfix.h"
#include "log.h"
#include "config.h"

#define MAX_INJECT_SLOTS 4

#define DECL_HOOK(name) \
    static tai_hook_ref_t name##_ref; \
    static SceUID name##_uid = -1

#define BIND_HOOK(name, nid) \
    name##_uid = taiHookFunctionExportForKernel( \
        KERNEL_PID, &name##_ref, "SceBt", TAI_ANY_LIBRARY, nid, name##_hook)

#define UNBIND_HOOK(name) \
    do { \
        if (name##_uid > 0) { \
            taiHookReleaseForKernel(name##_uid, name##_ref); \
            name##_uid = -1; \
        } \
    } while (0)

typedef struct {
    int used;
    int connected;
    int is_target;
    unsigned int mac0;
    unsigned int mac1;
    unsigned short vid;
    unsigned short pid;
    char name[128];
    int start_seen;
    int last_start_ret;
} DeviceState;

typedef struct {
    int used;
    unsigned int mac0;
    unsigned int mac1;
    SceUInt64 when;
} InjectJob;

static PluginConfig g_cfg;
static DeviceState g_dev[MAX_DEVICES];
static InjectJob g_jobs[MAX_INJECT_SLOTS];
static SceUID g_thread = -1;
static SceUID g_cb = -1;
static SceUID g_ts_uid = -1;
static volatile int g_stop;
static int g_ready;
static int g_hooked;
static int g_patch_tried;

static const unsigned char ts_inc_8000[4] = {
    TS_INC_8000_0, TS_INC_8000_1, TS_INC_8000_2, TS_INC_8000_3
};
static const unsigned char ts_inc_512[4] = {
    TS_INC_8000_0, TS_INC_8000_1, TS_INC_512_2, TS_INC_512_3
};

DECL_HOOK(ksceBtStartAudio);

static int ascii_icontains(const char *hay, const char *needle)
{
    unsigned int nlen = strlen(needle);
    unsigned int hlen;
    unsigned int i, j;

    if (!hay || !needle || nlen == 0)
        return 0;
    hlen = strlen(hay);
    if (hlen < nlen)
        return 0;
    for (i = 0; i + nlen <= hlen; i++) {
        for (j = 0; j < nlen; j++) {
            char a = hay[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z')
                a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z')
                b = (char)(b - 'A' + 'a');
            if (a != b)
                break;
        }
        if (j == nlen)
            return 1;
    }
    return 0;
}

static DeviceState *dev_find(unsigned int mac0, unsigned int mac1)
{
    int i;
    for (i = 0; i < MAX_DEVICES; i++) {
        if (g_dev[i].used && g_dev[i].mac0 == mac0 && g_dev[i].mac1 == mac1)
            return &g_dev[i];
    }
    return 0;
}

static DeviceState *dev_get(unsigned int mac0, unsigned int mac1)
{
    DeviceState *d = dev_find(mac0, mac1);
    int i;

    if (d)
        return d;
    for (i = 0; i < MAX_DEVICES; i++) {
        if (!g_dev[i].used) {
            memset(&g_dev[i], 0, sizeof(g_dev[i]));
            g_dev[i].used = 1;
            g_dev[i].mac0 = mac0;
            g_dev[i].mac1 = mac1;
            return &g_dev[i];
        }
    }
    return 0;
}

static int is_target_device(const DeviceState *d)
{
    if (!d)
        return 0;
    if (g_cfg.mac_count > 0)
        return mac_in_filter(&g_cfg, d->mac0, d->mac1);
    if (ascii_icontains(d->name, "AirPods"))
        return 1;
    if (d->vid == APPLE_VID)
        return 1;
    return 0;
}

static void fill_from_registry(DeviceState *d)
{
    SceBtRegisteredInfo info;
    unsigned short vp[2];
    int i, ret;

    if (ksceBtGetVidPid(d->mac0, d->mac1, vp) >= 0) {
        d->vid = vp[0];
        d->pid = vp[1];
    }

    for (i = 0; i < 16; i++) {
        memset(&info, 0, sizeof(info));
        ret = ksceBtGetRegisteredInfo(i, 0, &info, sizeof(info));
        if (ret < 0)
            break;
        if (info.mac[0] == (unsigned char)(d->mac0 & 0xFF)
            && info.mac[1] == (unsigned char)((d->mac0 >> 8) & 0xFF)
            && info.mac[2] == (unsigned char)((d->mac0 >> 16) & 0xFF)
            && info.mac[3] == (unsigned char)((d->mac0 >> 24) & 0xFF)
            && info.mac[4] == (unsigned char)(d->mac1 & 0xFF)
            && info.mac[5] == (unsigned char)((d->mac1 >> 8) & 0xFF)) {
            if (info.vid)
                d->vid = info.vid;
            if (info.pid)
                d->pid = info.pid;
            strncpy(d->name, info.name, sizeof(d->name) - 1);
            d->name[sizeof(d->name) - 1] = 0;
            break;
        }
    }

    if (d->name[0] == 0)
        ksceBtGetDeviceName(d->mac0, d->mac1, d->name);

    d->is_target = is_target_device(d);
}

static void log_device(const char *tag, const DeviceState *d)
{
    char mac[24];

    format_mac(mac, sizeof(mac), d->mac0, d->mac1);
    log_printf("%s mac=%s name='%s' target=%d", tag, mac, d->name, d->is_target);
}

static int bytes4_eq(const unsigned char *p, const unsigned char *q)
{
    return p[0] == q[0] && p[1] == q[1] && p[2] == q[2] && p[3] == q[3];
}

/* Find Thumb-2  adds.w r8, r8, #imm  in SceBt RX. #0x1F40 is the bug;
   #0x200 means we already patched this boot's image (or another plugin). */
static void install_ts_patch(void)
{
    tai_module_info_t tinfo;
    SceKernelModuleInfo minfo;
    const unsigned char *base;
    unsigned int memsz, off, found_8000, found_512, n8000, n512;
    int ret, seg;

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
    seg = 0;

    /* Segment 0 is RX code on every SceBt we have seen. */
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
        g_ts_uid = taiInjectDataForKernel(KERNEL_PID, tinfo.modid, seg,
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

static void schedule_inject(unsigned int mac0, unsigned int mac1, unsigned int delay_us)
{
    int i;
    SceUInt64 when = ksceKernelGetSystemTimeWide() + delay_us;

    for (i = 0; i < MAX_INJECT_SLOTS; i++) {
        if (!g_jobs[i].used) {
            g_jobs[i].used = 1;
            g_jobs[i].mac0 = mac0;
            g_jobs[i].mac1 = mac1;
            g_jobs[i].when = when;
            return;
        }
    }
}

static void do_inject(DeviceState *d)
{
    int ret, vol;

    if (!d || !d->is_target)
        return;

    vol = g_cfg.target_volume;
    if (vol < 0)
        vol = 0;
    if (vol > AVRCP_VOL_MAX)
        vol = AVRCP_VOL_MAX;

    if (g_cfg.force_start_audio && !(d->start_seen && d->last_start_ret >= 0)) {
        ret = ksceBtStartAudio((int)d->mac0, (int)d->mac1, 8, 0);
        d->last_start_ret = ret;
        d->start_seen = 1;
        if (g_cfg.debug)
            log_printf("StartAudio -> %s", bt_err_name(ret));
    }

    if (g_cfg.force_avrcp_volume) {
        ksceBtAvrcpReadVolume((int)d->mac0, (int)d->mac1, 0, 0);
        ret = ksceBtAvrcpSendVolume((int)d->mac0, (int)d->mac1, vol, 0);
        if (g_cfg.debug)
            log_printf("AvrcpSendVolume(%d) -> %s", vol, bt_err_name(ret));
    }
}

static void process_jobs(void)
{
    int i;
    SceUInt64 now = ksceKernelGetSystemTimeWide();

    for (i = 0; i < MAX_INJECT_SLOTS; i++) {
        if (!g_jobs[i].used)
            continue;
        if (now < g_jobs[i].when)
            continue;
        {
            DeviceState *d = dev_get(g_jobs[i].mac0, g_jobs[i].mac1);
            g_jobs[i].used = 0;
            if (d)
                do_inject(d);
        }
    }
}

static int ksceBtStartAudio_hook(int r0, int r1, int r2, int r3)
{
    int ret = TAI_CONTINUE(int, ksceBtStartAudio_ref, r0, r1, r2, r3);
    DeviceState *d = dev_get((unsigned int)r0, (unsigned int)r1);
    if (d) {
        d->start_seen = 1;
        d->last_start_ret = ret;
    }
    return ret;
}

static void on_connect(unsigned int mac0, unsigned int mac1)
{
    DeviceState *d = dev_get(mac0, mac1);
    if (!d)
        return;

    d->connected = 1;
    fill_from_registry(d);
    log_device("connect", d);

    if (d->is_target) {
        ksceBtSetContentProtection(0);
        schedule_inject(mac0, mac1, 400 * 1000);
        schedule_inject(mac0, mac1, 1200 * 1000);
        schedule_inject(mac0, mac1, 2500 * 1000);
    }
}

static void on_disconnect(unsigned int mac0, unsigned int mac1)
{
    DeviceState *d = dev_find(mac0, mac1);
    int i;

    if (d) {
        d->connected = 0;
        log_device("disconnect", d);
    }
    for (i = 0; i < MAX_INJECT_SLOTS; i++) {
        if (g_jobs[i].used && g_jobs[i].mac0 == mac0 && g_jobs[i].mac1 == mac1)
            g_jobs[i].used = 0;
    }
}

static int bluetooth_callback(int notifyId, int notifyCount, int notifyArg, void *common)
{
    SceBtEvent ev;
    int ret;

    (void)notifyId;
    (void)notifyCount;
    (void)notifyArg;
    (void)common;

    while ((ret = ksceBtReadEvent(&ev, 1)) == (int)SCE_BT_ERROR_CB_OVERFLOW)
        ;
    if (ret <= 0)
        return 0;

    switch (ev.id) {
    case BT_EVT_CONNECT:
        on_connect(ev.mac0, ev.mac1);
        break;
    case BT_EVT_DISCONNECT:
        on_disconnect(ev.mac0, ev.mac1);
        break;
    default:
        break;
    }
    return 0;
}

static int try_mount_ready(void)
{
    if (g_ready)
        return 1;
    if (log_init() < 0)
        return 0;
    config_load(&g_cfg);
    g_ready = 1;
    install_ts_patch();
    log_printf("vitabtfix.skprx %s ready debug=%d vol=%d patch=%s",
               PLUGIN_VERSION, g_cfg.debug, g_cfg.target_volume,
               g_ts_uid > 0 ? "on" : "off");
    return 1;
}

static int worker_thread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    g_cb = ksceKernelCreateCallback("vitabtfix_bt_cb", 0, bluetooth_callback, 0);
    if (g_cb >= 0)
        ksceBtRegisterCallback(g_cb, 0, 0xFFFFFFFF, 0xFFFFFFFF);

    while (!g_stop) {
        ksceKernelDelayThreadCB(100 * 1000);
        if (!try_mount_ready())
            continue;
        process_jobs();
    }

    if (g_cb >= 0) {
        ksceBtUnregisterCallback(g_cb);
        ksceKernelDeleteCallback(g_cb);
        g_cb = -1;
    }
    return 0;
}

static int install_hooks(void)
{
    BIND_HOOK(ksceBtStartAudio, NID_ksceBtStartAudio);
    g_hooked = 1;
    return 0;
}

static void remove_hooks(void)
{
    if (!g_hooked)
        return;
    UNBIND_HOOK(ksceBtStartAudio);
    if (g_ts_uid > 0) {
        taiInjectReleaseForKernel(g_ts_uid);
        g_ts_uid = -1;
    }
    g_hooked = 0;
}

int module_start(SceSize argc, const void *args)
{
    (void)argc;
    (void)args;

    memset(g_dev, 0, sizeof(g_dev));
    memset(g_jobs, 0, sizeof(g_jobs));
    config_set_defaults(&g_cfg);

    install_hooks();

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
    remove_hooks();
    log_shutdown();
    return SCE_KERNEL_STOP_SUCCESS;
}

void _start(void)
{
    module_start(0, 0);
}
