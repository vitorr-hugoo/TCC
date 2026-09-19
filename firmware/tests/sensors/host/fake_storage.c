#include "fake_storage.h"
#include <string.h>
char data_log[8192], health_log[8192];
unsigned write_count, short_at_write, fail_at_write;
FRESULT mount_result, close_result;
int fail_health_open;
static FATFS volume = {100000, 8};
void SPI1_DMA_Init(void)
{
}
void SD_Deselect(void)
{
}
FRESULT f_mkdir(const char *p)
{
    (void)p;
    return FR_OK;
}
FRESULT f_mount(FATFS *fs, const char *p, unsigned i)
{
    (void)fs;
    (void)p;
    (void)i;
    return mount_result;
}
FRESULT f_open(FIL *f, const char *p, unsigned mode)
{
    (void)mode;
    f->id = strstr(p, "sensor_health") != NULL;
    if (f->id && fail_health_open)
    {
        return FR_DISK_ERR;
    }
    f->size = (unsigned)strlen(f->id ? health_log : data_log);
    return FR_OK;
}
FRESULT f_write(FIL *f, const void *b, UINT n, UINT *written)
{
    ++write_count;
    *written = 0;
    if (write_count == fail_at_write)
    {
        return FR_DISK_ERR;
    }
    if (write_count == short_at_write)
    {
        --n;
    }
    char *dest = f->id ? health_log : data_log;
    size_t start = strlen(dest);
    if (start + n >= 8192)
    {
        return FR_DENIED;
    }
    memcpy(dest + start, b, n);
    dest[start + n] = 0;
    *written = n;
    f->size += n;
    return FR_OK;
}
FRESULT f_sync(FIL *f)
{
    (void)f;
    return FR_OK;
}
FRESULT f_close(FIL *f)
{
    (void)f;
    return close_result;
}
FRESULT f_getfree(const char *p, DWORD *count, FATFS **fs)
{
    (void)p;
    *count = 80000;
    *fs = &volume;
    return FR_OK;
}
