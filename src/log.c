#include <stdarg.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/stat.h>

#include "vitabtfix.h"
#include "log.h"

static SceUID g_mutex = -1;
static int g_ready;

static int ensure_dir(void)
{
    ksceIoMkdir("ux0:data", 6);
    ksceIoMkdir(PLUGIN_DIR, 6);
    return 0;
}

int log_init(void)
{
    SceIoStat st;
    SceUID fd;

    if (g_mutex < 0)
        g_mutex = ksceKernelCreateMutex("vitabtfix_log", 0, 0, 0);

    ensure_dir();

    if (ksceIoGetstat(PLUGIN_LOG, &st) >= 0 && st.st_size > (SceOff)(256 * 1024)) {
        ksceIoRemove("ux0:data/vitabtfix/log.old.txt");
        ksceIoRename(PLUGIN_LOG, "ux0:data/vitabtfix/log.old.txt");
    }

    fd = ksceIoOpen(PLUGIN_LOG, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 6);
    if (fd < 0)
        return fd;
    ksceIoClose(fd);
    g_ready = 1;
    return 0;
}

void log_shutdown(void)
{
    g_ready = 0;
    if (g_mutex >= 0) {
        ksceKernelDeleteMutex(g_mutex);
        g_mutex = -1;
    }
}

void log_printf(const char *fmt, ...)
{
    char buf[384];
    va_list ap;
    int n;
    SceUID fd;
    static unsigned int seq;

    if (!g_ready)
        return;

    n = snprintf(buf, 24, "[%u] ", ++seq);
    if (n < 0)
        n = 0;
    if (n > 23)
        n = 23;

    va_start(ap, fmt);
    n += vsnprintf(buf + n, sizeof(buf) - (unsigned int)n - 2, fmt, ap);
    va_end(ap);

    if (n < 0)
        return;
    if (n > (int)sizeof(buf) - 3)
        n = (int)sizeof(buf) - 3;
    buf[n++] = '\n';
    buf[n] = 0;

    if (g_mutex >= 0)
        ksceKernelLockMutex(g_mutex, 1, 0);

    fd = ksceIoOpen(PLUGIN_LOG, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 6);
    if (fd >= 0) {
        ksceIoWrite(fd, buf, (unsigned int)n);
        ksceIoClose(fd);
    }

    if (g_mutex >= 0)
        ksceKernelUnlockMutex(g_mutex, 1);
}

void format_mac(char *out, unsigned int n, unsigned int mac0, unsigned int mac1)
{
    snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X",
             (unsigned int)(mac0 & 0xFF),
             (unsigned int)((mac0 >> 8) & 0xFF),
             (unsigned int)((mac0 >> 16) & 0xFF),
             (unsigned int)((mac0 >> 24) & 0xFF),
             (unsigned int)(mac1 & 0xFF),
             (unsigned int)((mac1 >> 8) & 0xFF));
}

const char *bt_err_name(int err)
{
    switch ((unsigned int)err) {
    case 0: return "ok";
    case 0x802F0608: return "AVCTP_READ_NO_VOLUME";
    case 0x802F0609: return "AVCTP_SEND_NOT_RUBY";
    case 0x802F0D01: return "AUDIO_START_NOT_CONNECTED";
    case 0x802F0D02: return "AUDIO_START_NO_CAP";
    case 0x802F0D05: return "AUDIO_SEND_NOT_CONNECTED";
    case 0x802F0D06: return "AUDIO_SEND_NOT_STARTED";
    case 0x802F0D07: return "AUDIO_SEND_INVALID_LENGTH";
    case 0x802F0D0B: return "AUDIO_START_INVALID_SERV";
    case 0x802F0D0D: return "AUDIO_START_SERV_FAILED";
    case 0x802F0D11: return "AUDIO_SEND_BAD_TYPE";
    case 0x802F0D1B: return "AUDIO_SEND_NO_CAP";
    case 0x802F2205: return "AUDIO_START_IS_LE";
    case 0x802F2207: return "AUDIO_SEND_IS_LE";
    case 0x802F3403: return "NOT_SUPPORTED_DEVICE";
    case 0x802F3501: return "JEDI_VOLUME_GAIN_NOT_CONNECTED";
    default: return "err";
    }
}