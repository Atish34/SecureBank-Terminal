#include <lpc21xx.h>   /* adjust include to match your project setup */
#include "uart0_pc.h"

#define DLAB_BIT           7
#define WORD_LEN_SEL_BITS  0
#define _8BIT              3

#define U0LSR_RDR_BIT       0   /* Receiver Data Ready */
#define U0LSR_THRE_BIT      5   /* Transmit Holding Register Empty */

void UART0_Init(void)
{
    /* P0.0 = TxD0, P0.1 = RxD0 */
    PINSEL0 &= ~0x0000000F;
    PINSEL0 |=  0x00000005;

    U0LCR = ((1 << DLAB_BIT) | (_8BIT << WORD_LEN_SEL_BITS));

    /* NOTE: 97 assumes a specific PCLK for 9600 baud — this MUST match
       pc_main.c's BAUD_RATE (9600). Recalculate this divisor from your
       actual PCLK: divisor = PCLK / (16 * baud). Verify in your board's
       clock config before flashing — do not assume this value is right
       for your setup. */
    U0DLL = 97;
    U0DLM = 0;

    U0LCR &= ~(1 << DLAB_BIT);
}

void UART0_SendChar(char c)
{
    while (!(U0LSR & (1 << U0LSR_THRE_BIT)))
        ;                       /* wait until transmitter is ready */
    U0THR = c;
}

void UART0_SendString(const char *str)
{
    while (*str != '\0')
    {
        UART0_SendChar(*str);
        str++;
    }
}

int UART0_ReceiveString(char *buf, int buf_size, unsigned long timeout_loops)
{
    int idx = 0;
    unsigned long waited;
	char c;

    while (idx < buf_size - 1)
    {
        waited = 0;

        /* Wait for a byte to arrive, or give up after timeout_loops */
        while (!(U0LSR & (1 << U0LSR_RDR_BIT)))
        {
            waited++;
            if (waited >= timeout_loops)
                return -1;      /* timed out waiting for a byte */
        }

        c = (char) U0RBR;

        if (c == '$')           /* end-of-message marker, per protocol */
            break;

        buf[idx++] = c;
    }

    buf[idx] = '\0';
    return idx;
}
