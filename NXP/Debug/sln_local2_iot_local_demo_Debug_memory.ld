/*
 * GENERATED FILE - DO NOT EDIT
 * Copyright (c) 2008-2013 Code Red Technologies Ltd,
 * Copyright 2015, 2018-2019 NXP
 * (c) NXP Semiconductors 2013-2022
 * Generated linker script file for MIMXRT106SxxxxA
 * Created from memory.ldt by FMCreateLinkMemory
 * Using Freemarker v2.3.30
 * MCUXpresso IDE v11.3.1 [Build 5262] [2021-04-02] on 2022/3/12 ¤U¤È 03:37:28
 */

MEMORY
{
  /* Define each memory region */
  BOARD_FLASH (rx) : ORIGIN = 0x60d00000, LENGTH = 0xa00000 /* 10M bytes (alias Flash) */  
  SRAM_DTC (rwx) : ORIGIN = 0x20000000, LENGTH = 0x78000 /* 480K bytes (alias RAM) */  
  SRAM_ITC (rwx) : ORIGIN = 0x0, LENGTH = 0x8000 /* 32K bytes (alias RAM2) */  
  SRAM_OC_NON_CACHEABLE (rwx) : ORIGIN = 0x20200000, LENGTH = 0x40000 /* 256K bytes (alias RAM3) */  
  SRAM_OC_CACHEABLE (rwx) : ORIGIN = 0x20240000, LENGTH = 0x40000 /* 256K bytes (alias RAM4) */  
}

  /* Define a symbol for the top of each memory region */
  __base_BOARD_FLASH = 0x60d00000  ; /* BOARD_FLASH */  
  __base_Flash = 0x60d00000 ; /* Flash */  
  __top_BOARD_FLASH = 0x60d00000 + 0xa00000 ; /* 10M bytes */  
  __top_Flash = 0x60d00000 + 0xa00000 ; /* 10M bytes */  
  __base_SRAM_DTC = 0x20000000  ; /* SRAM_DTC */  
  __base_RAM = 0x20000000 ; /* RAM */  
  __top_SRAM_DTC = 0x20000000 + 0x78000 ; /* 480K bytes */  
  __top_RAM = 0x20000000 + 0x78000 ; /* 480K bytes */  
  __base_SRAM_ITC = 0x0  ; /* SRAM_ITC */  
  __base_RAM2 = 0x0 ; /* RAM2 */  
  __top_SRAM_ITC = 0x0 + 0x8000 ; /* 32K bytes */  
  __top_RAM2 = 0x0 + 0x8000 ; /* 32K bytes */  
  __base_SRAM_OC_NON_CACHEABLE = 0x20200000  ; /* SRAM_OC_NON_CACHEABLE */  
  __base_RAM3 = 0x20200000 ; /* RAM3 */  
  __top_SRAM_OC_NON_CACHEABLE = 0x20200000 + 0x40000 ; /* 256K bytes */  
  __top_RAM3 = 0x20200000 + 0x40000 ; /* 256K bytes */  
  __base_SRAM_OC_CACHEABLE = 0x20240000  ; /* SRAM_OC_CACHEABLE */  
  __base_RAM4 = 0x20240000 ; /* RAM4 */  
  __top_SRAM_OC_CACHEABLE = 0x20240000 + 0x40000 ; /* 256K bytes */  
  __top_RAM4 = 0x20240000 + 0x40000 ; /* 256K bytes */  
