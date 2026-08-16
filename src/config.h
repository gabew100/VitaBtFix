#ifndef VITABTFIX_CONFIG_H
#define VITABTFIX_CONFIG_H

#include "vitabtfix.h"

typedef struct {
    int debug;
    int target_volume;
    int force_start_audio;
    int force_avrcp_volume;
    int mac_count;
    unsigned int mac0[MAX_MAC_FILTERS];
    unsigned int mac1[MAX_MAC_FILTERS];
} PluginConfig;

void config_set_defaults(PluginConfig *cfg);
int config_load(PluginConfig *cfg);
int config_write_default(void);
int mac_in_filter(const PluginConfig *cfg, unsigned int mac0, unsigned int mac1);
int parse_mac(const char *s, unsigned int *mac0, unsigned int *mac1);

#endif
