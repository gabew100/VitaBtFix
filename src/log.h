#ifndef VITABTFIX_LOG_H
#define VITABTFIX_LOG_H

int log_init(void);
void log_shutdown(void);
void log_printf(const char *fmt, ...);
const char *bt_err_name(int err);
void format_mac(char *out, unsigned int n, unsigned int mac0, unsigned int mac1);

#endif