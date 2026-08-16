#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/stat.h>

#include "config.h"
#include "log.h"

static const char *k_default_cfg =
    "# vitabtfix.skprx 1.0\n"
    "debug=0\n"
    "target_volume=100\n"
    "force_start_audio=1\n"
    "force_avrcp_volume=1\n"
    "# Optional MAC allow-list (repeat mac=). Empty = any AirPods / Apple VID.\n"
    "# mac=AA:BB:CC:DD:EE:FF\n";

void config_set_defaults(PluginConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->debug = 0;
    cfg->target_volume = 100;
    cfg->force_start_audio = 1;
    cfg->force_avrcp_volume = 1;
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

int parse_mac(const char *s, unsigned int *mac0, unsigned int *mac1)
{
    unsigned char b[6];
    int i, hi, lo;
    const char *p = s;

    for (i = 0; i < 6; i++) {
        while (*p == ' ' || *p == ':' || *p == '-')
            p++;
        hi = hex_nibble(*p++);
        lo = hex_nibble(*p++);
        if (hi < 0 || lo < 0)
            return -1;
        b[i] = (unsigned char)((hi << 4) | lo);
    }

    *mac0 = (unsigned int)b[0]
          | ((unsigned int)b[1] << 8)
          | ((unsigned int)b[2] << 16)
          | ((unsigned int)b[3] << 24);
    *mac1 = (unsigned int)b[4] | ((unsigned int)b[5] << 8);
    return 0;
}

int mac_in_filter(const PluginConfig *cfg, unsigned int mac0, unsigned int mac1)
{
    int i;
    if (cfg->mac_count <= 0)
        return 0;
    for (i = 0; i < cfg->mac_count; i++) {
        if (cfg->mac0[i] == mac0 && cfg->mac1[i] == mac1)
            return 1;
    }
    return 0;
}

static void apply_line(PluginConfig *cfg, char *line)
{
    char *eq, *key, *val;

    while (*line == ' ' || *line == '\t')
        line++;
    if (*line == 0 || *line == '#' || *line == '\r')
        return;

    eq = strchr(line, '=');
    if (!eq)
        return;
    *eq = 0;
    key = line;
    val = eq + 1;
    while (*val == ' ' || *val == '\t')
        val++;

    {
        char *end = val + strlen(val);
        while (end > val && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ')) {
            end[-1] = 0;
            end--;
        }
    }

    if (strcmp(key, "debug") == 0)
        cfg->debug = (int)strtol(val, 0, 10);
    else if (strcmp(key, "target_volume") == 0) {
        int v = (int)strtol(val, 0, 10);
        if (v < 0)
            v = 0;
        if (v > AVRCP_VOL_MAX)
            v = AVRCP_VOL_MAX;
        cfg->target_volume = v;
    } else if (strcmp(key, "force_start_audio") == 0)
        cfg->force_start_audio = (int)strtol(val, 0, 10);
    else if (strcmp(key, "force_avrcp_volume") == 0)
        cfg->force_avrcp_volume = (int)strtol(val, 0, 10);
    else if (strcmp(key, "mac") == 0 && cfg->mac_count < MAX_MAC_FILTERS) {
        if (parse_mac(val, &cfg->mac0[cfg->mac_count], &cfg->mac1[cfg->mac_count]) == 0)
            cfg->mac_count++;
    }
}

int config_write_default(void)
{
    SceUID fd;

    ksceIoMkdir("ux0:data", 6);
    ksceIoMkdir(PLUGIN_DIR, 6);

    fd = ksceIoOpen(PLUGIN_CFG, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 6);
    if (fd < 0)
        return fd;
    ksceIoWrite(fd, k_default_cfg, strlen(k_default_cfg));
    ksceIoClose(fd);
    return 0;
}

int config_load(PluginConfig *cfg)
{
    char buf[1024];
    char line[160];
    int n, i, li;
    SceUID fd;

    config_set_defaults(cfg);

    fd = ksceIoOpen(PLUGIN_CFG, SCE_O_RDONLY, 0);
    if (fd < 0) {
        config_write_default();
        return 0;
    }

    n = ksceIoRead(fd, buf, sizeof(buf) - 1);
    ksceIoClose(fd);
    if (n < 0)
        return n;
    buf[n] = 0;

    li = 0;
    for (i = 0; i <= n; i++) {
        char c = buf[i];
        if (c == '\n' || c == 0) {
            line[li] = 0;
            apply_line(cfg, line);
            li = 0;
            if (c == 0)
                break;
        } else if (li < (int)sizeof(line) - 1) {
            line[li++] = c;
        }
    }
    return 0;
}