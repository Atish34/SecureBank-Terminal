/*
 * rfid_uart1.c — RFID reader interface for SecureBank Terminal
 *
 * Changes made vs. the original draft:
 *   1. Moved RFID handling from UART0 -> UART1 (per block diagram:
 *      RFID reader -> UART1 RX, PC link -> UART0). Rename back if your
 *      actual wiring is different, but keep it CONSISTENT with PINSEL
 *      and with whichever UART your PC-comm code uses.
 *   2. Added VIC vector/interrupt enable setup (was missing before —
 *      without this the ISR is never actually called).
 *   3. Added frame-length validation: only accept exactly 8-digit
 *      card numbers before setting rfid_ready, so a corrupted/partial
 *      frame isn't treated as a valid read.
 *   4. Added an example of clearing rfid_ready in the consumer code,
 *      since the ISR sets it but never clears it.
 */

#include <lpc21xx.h>   /* adjust include to match your project setup */

typedef unsigned char  u8;
typedef unsigned int   u32;

#define DLAB_BIT           7
#define WORD_LEN_SEL_BITS  0
#define _8BIT              3

#define RFID_FRAME_LEN     8   /* 8-digit card number, per spec */

volatile unsigned char rfid_buffer[16];
volatile u8 rfid_index = 0;
volatile u8 rfid_ready = 0;
void UART1_ISR(void) __irq;

/* ────────── UART1 Init (RFID reader line) ────────── */
void UART1_Init(void)
{
    /* Configure P0.8/P0.9 (or whichever pins map to UART1 RX/TX on your
       board) as UART1 function via PINSEL0/PINSEL1 — confirm against
       your board's pin mapping before running. */
    PINSEL0 &= ~0x000F0000;
    PINSEL0 |=  0x00050000;      /* example: TxD1/RxD1 select bits */

    U1LCR = ((1 << DLAB_BIT) | (_8BIT << WORD_LEN_SEL_BITS));
    U1DLL = 97;                  /* baud rate divisor — must match PC side */
    U1DLM = 0;
    U1LCR &= ~(1 << DLAB_BIT);

    U1IER = 0x01;                /* enable UART1 RX interrupt */

    /* ---- VIC setup: THIS WAS MISSING BEFORE ----
       Without assigning a VIC slot and unmasking the interrupt source,
       U1IER alone will not route the interrupt to UART1_ISR. */
    VICVectAddr6  = (u32)UART1_ISR;  /* UART1 = VIC channel 7 on LPC2148 —
                                         double check against your datasheet;
                                         some boards use VICVectAddr7 */
    VICVectCntl6  = 0x20 | 7;        /* 7 = UART1 interrupt source, slot enabled */
    VICIntEnable  = (1 << 7);        /* unmask UART1 interrupt globally */
}

/* ────────── UART1 ISR (RFID frame parsing) ────────── */
void UART1_ISR(void) __irq
{
    u8 iir = U1IIR & 0x0E;

    if (iir == 0x04 || iir == 0x0C)
    {
        u8 data = U1RBR;

        if (data == 0x02)              /* STX — start of frame */
        {
            rfid_index = 0;
            rfid_ready = 0;
        }
        else if (data == 0x03)         /* ETX — end of frame */
        {
            /* Only accept a frame if it's exactly 8 digits (card number
               length per spec). Anything shorter/longer is discarded
               as corrupted/partial, rather than passed on as valid. */
            if (rfid_index == RFID_FRAME_LEN)
            {
                rfid_buffer[rfid_index] = '\0';
                rfid_ready = 1;
            }
            rfid_index = 0;             /* reset either way, ready for next frame */
        }
        else if (rfid_index < 12)
        {
            rfid_buffer[rfid_index++] = data;
        }
    }

    VICVectAddr = 0x00;                /* acknowledge interrupt to VIC */
}

/* ────────── Example consumer usage (in your main loop) ──────────
   This shows WHERE rfid_ready must be cleared — it is set by the ISR
   but nothing clears it automatically. Put this in main.c, not here. */
/*
while (1)
{
    if (rfid_ready)
    {
        // rfid_buffer now holds an 8-digit, null-terminated card number
        // e.g. build and send "#CARD:12345678$" to PC over UART0 here

        rfid_ready = 0;   // IMPORTANT: clear it, or you'll reprocess
                          // the same card read on every loop iteration
    }
}
*/
