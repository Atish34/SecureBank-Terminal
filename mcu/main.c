/*
 * main.c — SecureBank Terminal MCU state machine
 *
 * Rewritten to build protocol strings with strcpy()/strcat() instead of
 * snprintf() — avoids depending on whether your Keil C library includes
 * snprintf. Number-to-string conversion is done with a small custom
 * helper (u32_to_str), same digit-extraction idea as your U32LCD().
 *
 * Not compiled/tested here — no ARM toolchain available in this
 * environment, and this depends on your types.h / lcd_defines.h /
 * KPM_defines.h which I have not seen. Build in Keil and check the
 * compiler output before flashing.
 */

#include <lpc21xx.h>
#include "types.h"          /* your u8/u32/s8/s32/f32 typedefs */
#include "define.h"
#include "delay.h"
#include "lcd.h"
#include "lcd_defines.h"    /* for CLEAR_LCD etc. */
#include "KPM_defines.h"
#include <string.h>

#include "uart0_pc.h"

/* ---- From rfid_uart1.c ---- */
extern volatile unsigned char rfid_buffer[16];
extern volatile u8 rfid_ready;
void UART1_Init(void);
static u8 menu_page = 1;

/* ---- From KPM.c / lcd.c (your real modules) ---- */
void Init_KPM(void);
u32  keyscan(void);
u32  ReadNum(void);          /* blocks; returns -1 (as u32, i.e. 0xFFFFFFFF)
                                 if 'E' pressed with no digits entered */
void InitLCD(void);
void CmdLCD(u8 cmd);
void StrLCD(s8 *str);
void U32LCD(u32 n);

#define UART0_TIMEOUT_LOOPS   2000000UL   /* tune against your clock speed */

typedef enum {
    STATE_IDLE,
    STATE_WAIT_PIN,
    STATE_MENU
} State;

static State state = STATE_IDLE;
static char  current_card[20] = "";

/* Small helper since StrLCD wants s8* not const char* */
static void lcd_print(const char *msg)
{
    StrLCD((s8 *)msg);
}

static void show_page1(void)
{
    CmdLCD(CLEAR_LCD);
    lcd_print("1:Bal 2:Dep 3:Wd");
    CmdLCD(GOTO_LINE2_POS0);
    lcd_print("4:Next *:Exit");
}

static void show_page2(void)
{
    CmdLCD(CLEAR_LCD);
    lcd_print("5:Mini 6:PIN");
    CmdLCD(GOTO_LINE2_POS0);
    lcd_print("4:Back *:Exit");
}

static void show_menu(void)
{
    if (menu_page == 1)
        show_page1();
    else
        show_page2();
}

static void exit_from_activity(void)
{
	  CmdLCD(CLEAR_LCD);
    lcd_print("Going To Menu...");
		delay_ms(500);
    state = STATE_MENU;
}

/* ---- Convert an unsigned number to a decimal string.
   Same digit-extraction logic as your U32LCD(), just written into a
   buffer instead of sent to the LCD. buf must be big enough (10+1 is
   safe for any u32 value). ---- */
static void u32_to_str(u32 n, char *buf)
{
    char temp[10];
    int  i = 0, j = 0;

    if (n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (n > 0) {
        temp[i++] = (n % 10) + '0';
        n /= 10;
    }

    /* digits came out reversed, so write them back in order */
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

/* ---- Convert a number to a fixed-width, zero-padded string.
   Used for the PIN, which must always come out as exactly `width`
   digits (e.g. entering 0093 must produce "0093", not "93"). ---- */
static void u32_to_padded_str(u32 n, char *buf, int width)
{
    char raw[10];
    int  raw_len, pad, i;

    u32_to_str(n, raw);
    raw_len = strlen(raw);
    pad = width - raw_len;
    if (pad < 0) pad = 0;

    for (i = 0; i < pad; i++)
        buf[i] = '0';

    strcpy(buf + pad, raw);
}

/* ---- Read a PIN using your existing ReadNum(). Returns 1 on success
   (pin_out filled with exactly `width` digits), 0 if cancelled. ---- */
static int read_pin(char *pin_out, int width)
{
    s32 val = (s32) ReadNum();     /* -1 as u32 reinterprets as -1 in s32 */
    if (val == -1)
        return 0;                  /* cancelled */

    u32_to_padded_str((u32) val, pin_out, width);
    return 1;
}

/* ---- Send a request, wait for reply ---- */
static int send_and_wait(const char *request, char *out_buf, int out_size)
{
	int len;
    UART0_SendString(request);
    len = UART0_ReceiveString(out_buf, out_size, UART0_TIMEOUT_LOOPS);
    if (len < 0) {
        CmdLCD(CLEAR_LCD);
        lcd_print("PC not responding");
        delay_ms(1500);
        CmdLCD(CLEAR_LCD);
        lcd_print("Tap Card...");
        return 0;
    }
    return 1;
}

static void handle_idle(void)
{
	char reply[64];
    char request[32];
    if (!rfid_ready)
        return;

    /* Build "#CARD:xxxxxxxx$" with strcpy/strcat instead of snprintf */
	
    strcpy(request, "#CARD:");
    strcat(request, (const char *)rfid_buffer);
    strcat(request, "$");

    strcpy(current_card, (const char *)rfid_buffer);

    rfid_ready = 0;   /* clear it now that we've consumed it */

  
    if (!send_and_wait(request, reply, sizeof reply))
        return;   /* stay IDLE on timeout */

    if (strncmp(reply, "@OK", 3) == 0) {
        CmdLCD(CLEAR_LCD);
        lcd_print("Enter PIN:");
        state = STATE_WAIT_PIN;
    } else {
        CmdLCD(CLEAR_LCD);
        lcd_print("Invalid Card");
        delay_ms(1500);
        CmdLCD(CLEAR_LCD);
        lcd_print("Tap Card...");
        state = STATE_IDLE;
    }
}

static void handle_wait_pin(void)
{
    char pin[10];
	char request[40];
	char reply[64];

    if (!read_pin(pin, 4)) {
        CmdLCD(CLEAR_LCD);
        lcd_print("Cancelled");
        delay_ms(1000);
        CmdLCD(CLEAR_LCD);
        lcd_print("Tap Card...");
        state = STATE_IDLE;
        return;
    }

    /* Build "#CARD:xxxxxxxx#PIN:nnnn$" with strcpy/strcat */
   
    strcpy(request, "#CARD:");
    strcat(request, current_card);
    strcat(request, "#PIN:");
    strcat(request, pin);
    strcat(request, "$");

 
    if (!send_and_wait(request, reply, sizeof reply)) {
        state = STATE_IDLE;
        return;
    }

    if (strncmp(reply, "@OK", 3) == 0) {
    state = STATE_MENU;
    menu_page = 1;
    show_menu();	
    } else {
        CmdLCD(CLEAR_LCD);
        lcd_print("Invalid PIN");
        delay_ms(1500);
        CmdLCD(CLEAR_LCD);
        lcd_print("Tap Card...");
        state = STATE_IDLE;
    }
}

static void handle_menu(void)
{
    u32  key = keyscan();      /* single keypress: menu selection */
    char request[40] = "";
    char reply[64];
    char amount_str[10];
    s32  amt;

    switch ((char) key) {
        case '1':
            strcpy(request, "#TXN:BALANCE#REQ$");
            break;

        case '2':
            CmdLCD(CLEAR_LCD);
            lcd_print("Enter amount:");
            amt = (s32) ReadNum();
            if (amt == -1) return;   /* cancelled, stay in menu */

            u32_to_str((u32) amt, amount_str);
            strcpy(request, "#TXN:DEPOSIT#AMOUNT:");
            strcat(request, amount_str);
            strcat(request, "$");
            break;

        case '3':
            CmdLCD(CLEAR_LCD);
            lcd_print("Enter amount:");
            amt = (s32) ReadNum();
            if (amt == -1) return;

            u32_to_str((u32) amt, amount_str);
            strcpy(request, "#TXN:WITHDRAW#AMOUNT:");
            strcat(request, amount_str);
            strcat(request, "$");
            break;
				
				case '4':
            if (menu_page == 1) {
                 menu_page = 2;
            } else {
                 menu_page = 1;
            }
                 show_menu();
             return;

        case '*':   /* using '*' from your KPM layout as EXIT */
            CmdLCD(CLEAR_LCD);
            lcd_print("EXITING...");
				    delay_ms(500);
            state = STATE_IDLE;
            return;
									   
        default:
            return;   /* ignore unrecognized key, stay in menu */
    }

    if (!send_and_wait(request, reply, sizeof reply)) {
        state = STATE_IDLE;
        return;
    }

    CmdLCD(CLEAR_LCD);
    lcd_print(reply);   /* raw reply for now — format/parse further if needed */

    state = STATE_MENU;   /* loop back to menu after showing result */
}

int main(void)
{
    UART0_Init();     /* PC link */
    UART1_Init();     /* RFID reader */
    InitLCD();
    Init_KPM();

    CmdLCD(CLEAR_LCD);
    StrLCD("SecureBank Term.");
    delay_s(1);
    CmdLCD(CLEAR_LCD);
    lcd_print("Tap Card...");

    while (1)
    {															                                                                                                                                                                                    
        switch (state) {
            case STATE_IDLE:     handle_idle();     break;
            case STATE_WAIT_PIN: handle_wait_pin(); break;
            case STATE_MENU:     handle_menu();     break;
        }
    }

    return 0;
}
