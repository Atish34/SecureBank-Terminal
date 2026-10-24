/*
 * uart0_pc.h / uart0_pc.c — UART0 driver for MCU <-> PC link
 * (RFID stays on UART1 — see rfid_uart1.c from earlier)
 *
 * This is polling-based (not interrupt-driven) on purpose: the protocol
 * is strictly request/reply — the MCU sends one message, then MUST wait
 * for the PC's reply before doing anything else — so blocking here is
 * simpler and correct. UART1 (RFID) stays interrupt-driven because RFID
 * data can arrive at any time, independent of what the MCU is doing.
 */

#ifndef _UART0_PC_H_
#define _UART0_PC_H_

void UART0_Init(void);
void UART0_SendChar(char c);
void UART0_SendString(const char *str);

/* Blocks until '$' is received or timeout_loops is exhausted.
   Returns number of characters stored (excluding the null terminator),
   or -1 on timeout. buf is null-terminated on success. */
int  UART0_ReceiveString(char *buf, int buf_size, unsigned long timeout_loops);

#endif
