#ifndef FAKE_FF_H
#define FAKE_FF_H
#include <stdint.h>
typedef enum
{
    FR_OK,
    FR_DISK_ERR,
    FR_INT_ERR,
    FR_NOT_READY,
    FR_NO_FILE,
    FR_NO_PATH,
    FR_INVALID_NAME,
    FR_DENIED,
    FR_EXIST,
    FR_INVALID_OBJECT,
    FR_WRITE_PROTECTED,
    FR_INVALID_DRIVE,
    FR_NOT_ENABLED,
    FR_NO_FILESYSTEM,
    FR_MKFS_ABORTED,
    FR_TIMEOUT,
    FR_LOCKED,
    FR_NOT_ENOUGH_CORE,
    FR_TOO_MANY_OPEN_FILES,
    FR_INVALID_PARAMETER
} FRESULT;
typedef unsigned UINT;
typedef uint32_t DWORD;
typedef struct
{
    uint32_t n_fatent, csize;
} FATFS;
typedef struct
{
    unsigned id, size;
} FIL;
#define FA_WRITE       2
#define FA_READ        1
#define FA_OPEN_APPEND 0x30
#define f_size(f)      ((f)->size)
FRESULT f_mkdir(const char *p);
FRESULT f_open(FIL *f, const char *p, unsigned mode);
FRESULT f_write(FIL *f, const void *buf, UINT len, UINT *written);
FRESULT f_sync(FIL *f);
FRESULT f_close(FIL *f);
FRESULT f_mount(FATFS *fs, const char *path, unsigned immediate);
FRESULT f_getfree(const char *p, DWORD *count, FATFS **fs);
#endif
