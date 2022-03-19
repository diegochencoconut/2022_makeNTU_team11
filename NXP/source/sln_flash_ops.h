/*
 * Copyright 2020 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SLN_FLASH_OPS_
#define _SLN_FLASH_OPS_

#include "flexspi_hyper_flash_ops.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define sln_flash_ops_get_flash_id(a, b)    flexspi_nor_hyperflash_id(a, b)
#define sln_flash_ops_page_program(a, b, c) flexspi_nor_flash_page_program_with_buffer(a, b, c)
/*******************************************************************************
 * Prototypes
 ******************************************************************************/

#endif /* _SLN_FLASH_OPS__ */
