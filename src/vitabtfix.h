#ifndef VITABTFIX_H
#define VITABTFIX_H

#define PLUGIN_VERSION "1.0"
#define PLUGIN_DIR     "ux0:data/vitabtfix"
#define PLUGIN_LOG     "ux0:data/vitabtfix/log.txt"
#define PLUGIN_CFG     "ux0:data/vitabtfix/config.txt"

#define APPLE_VID       0x004C
#define MAX_DEVICES     4
#define MAX_MAC_FILTERS 8
#define AVRCP_VOL_MAX   127

#define NID_ksceBtStartAudio      0x8D47CABD
#define NID_ksceBtAvrcpSendVolume 0x7689DA3D
#define NID_ksceBtAvrcpReadVolume 0xC9C70056

#define BT_EVT_CONNECT    0x05
#define BT_EVT_DISCONNECT 0x06

/* Thumb-2  adds.w r8, r8, #0x1F40  after each A2DP media send.
   Same encoding on 3.65 at seg0+0x785C; other firmware is found
   by scanning SceBt RX rather than assuming that offset. */
#define TS_INC_8000_0  0x18
#define TS_INC_8000_1  0xF5
#define TS_INC_8000_2  0xFA
#define TS_INC_8000_3  0x58
#define TS_INC_512_2   0x00
#define TS_INC_512_3   0x78

#endif
