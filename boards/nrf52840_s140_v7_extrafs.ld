/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx)     : ORIGIN = 0x27000, LENGTH = 0xD4000 - 0x27000

  /* SRAM required by Softdevice depend on
   * - Attribute Table Size (Number of Services and Characteristics)
   * - Vendor UUID count
   * - Max ATT MTU
   * - Concurrent connection peripheral + central + secure links
   * - Event Len, HVN queue, Write CMD queue
   */ 
  RAM (rwx) :  ORIGIN = 0x20006000, LENGTH = 0x20040000 - 0x20006000
}

SECTIONS
{
  . = ALIGN(4);
  .svc_data :
  {
    PROVIDE(__start_svc_data = .);
    KEEP(*(.svc_data))
    PROVIDE(__stop_svc_data = .);
  } > RAM
  
  .fs_data :
  {
    PROVIDE(__start_fs_data = .);
    KEEP(*(.fs_data))
    PROVIDE(__stop_fs_data = .);
  } > RAM
} INSERT AFTER .data;

/* DL9SAU 2026-07-09: no-init RAM fuer den Crash-Faenger (PC/LR/CFSR ueber
 * Soft-Reset retten). Liegt HINTER .bss -> ausserhalb der Startup-.bss-Zero-
 * Schleife (__bss_start__..__bss_end__), wird also nicht genullt und ueberlebt
 * NVIC_SystemReset. NOLOAD: kein Flash-Load-Image. Siehe NRF52Board.cpp
 * (g_crash_info, section ".noinit"). */
SECTIONS
{
  .noinit (NOLOAD):
  {
    . = ALIGN(4);
    PROVIDE(__start_noinit = .);
    KEEP(*(.noinit))
    KEEP(*(.noinit.*))
    . = ALIGN(4);
    PROVIDE(__stop_noinit = .);
  } > RAM
} INSERT AFTER .bss;

INCLUDE "nrf52_common.ld"
