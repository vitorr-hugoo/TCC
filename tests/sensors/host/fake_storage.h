#include "ff.h"
extern char data_log[8192], health_log[8192];
extern unsigned write_count, short_at_write, fail_at_write;
extern FRESULT mount_result, close_result;
extern int fail_health_open;
