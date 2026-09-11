/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   This file includes a diskio driver skeleton to be completed by the user.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
 /* USER CODE END Header */

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/*
 * Warning: the user section 0 is no more in use (starting from CubeMx version 4.16.0)
 * To be suppressed in the future.
 * Kept to ensure backward compatibility with previous CubeMx versions when
 * migrating projects.
 * User code previously added there should be copied in the new user sections before
 * the section contents can be deleted.
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */

/* Includes ------------------------------------------------------------------*/
#include "ff_gen_drv.h"
#include "sd_card.h"
#include <stdio.h>
#include <string.h>
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;
SD_Handle_t hsd;

static void USER_mark_not_ready(void)
{
    hsd.initialized = false;
    Stat |= STA_NOINIT;
}
/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN INIT */

    if (pdrv != 0u)
    {
        return STA_NOINIT;
    }

    USER_mark_not_ready();
    memset(&hsd, 0, sizeof(hsd));

    SD_Status_t status = SD_SPI_InitCard(&hsd);
    if (status == SD_OK)
    {
        Stat &= (DSTATUS)~STA_NOINIT;
    }
    else
    {
        printf("[DISKIO] Inicializacao falhou: %s\r\n", SD_StatusStr(status));
    }
    return Stat;

  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */

    if (pdrv != 0u)
    {
        return STA_NOINIT;
    }

    if (!hsd.initialized)
    {
        Stat |= STA_NOINIT;
    }
    return Stat;

  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */

    if ((pdrv != 0u) || (buff == NULL) || (count == 0u))
    {
        return RES_PARERR;
    }
    if ((Stat & STA_NOINIT) || !hsd.initialized)
    {
        return RES_NOTRDY;
    }

    for (UINT i = 0u; i < count; i++)
    {
        SD_Status_t status = SD_SPI_ReadBlock(&hsd, sector + i,
                                               buff + (i * 512u));
        if (status != SD_OK)
        {
            if (status == SD_ERR_PARAM) return RES_PARERR;
            USER_mark_not_ready();
            return RES_ERROR;
        }
    }
    return RES_OK;

  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */

    if ((pdrv != 0u) || (buff == NULL) || (count == 0u))
    {
        return RES_PARERR;
    }
    if ((Stat & STA_NOINIT) || !hsd.initialized)
    {
        return RES_NOTRDY;
    }

    for (UINT i = 0u; i < count; i++)
    {
        SD_Status_t status = SD_SPI_WriteBlock(&hsd, sector + i,
                                                buff + (i * 512u));
        if (status != SD_OK)
        {
            if (status == SD_ERR_PARAM) return RES_PARERR;
            USER_mark_not_ready();
            return RES_ERROR;
        }
    }
    return RES_OK;

  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */

    if (pdrv != 0u) return RES_PARERR;
    if ((Stat & STA_NOINIT) || !hsd.initialized) return RES_NOTRDY;

    switch (cmd) {
        case CTRL_SYNC:
            if (SD_SPI_Sync(&hsd) == SD_OK) return RES_OK;
            USER_mark_not_ready();
            return RES_ERROR;

        case GET_SECTOR_COUNT:
            if ((buff == NULL) || (hsd.sector_count == 0u)) return RES_PARERR;
            *(DWORD*)buff = (DWORD)hsd.sector_count;
            return RES_OK;

        case GET_SECTOR_SIZE:
            if (buff == NULL) return RES_PARERR;
            *(WORD*)buff = 512; // Padrão absoluto de cartões SD
            return RES_OK;

        case GET_BLOCK_SIZE:
            if (buff == NULL) return RES_PARERR;
            // Tamanho do bloco de apagamento. Retornar 1 é seguro.
            *(DWORD*)buff = 1;
            return RES_OK;

        default:
            return RES_PARERR;
    }

  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

