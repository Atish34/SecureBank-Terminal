
/*
 * main.c � SecureBank Terminal MCU
 *
 * Existing logic preserved:
 * - RFID card verification
 * - PIN verification using ReadNum()
 * - Balance inquiry
 * - Deposit
 * - Withdrawal
 *
 * Added:
 * - Two-page menu
 * - Exit using E or *
 * - PIN change request
 * - Mini-statement request
 * - Activity result screen with menu exit
 */

#include <lpc21xx.h>
#include <string.h>

#include "types.h"
#include "define.h"
#include "delay.h"
#include "lcd.h"
#include "lcd_defines.h"
#include "KPM_defines.h"
#include "uart0_pc.h"

/* RFID module */
extern volatile unsigned char rfid_buffer[16];
extern volatile u8 rfid_ready;

void UART1_Init(void);

/* Keypad and LCD functions */
void Init_KPM(void);
u32 keyscan(void);
u32 ReadNum(void);

void InitLCD(void);
void CmdLCD(u8 cmd);
void StrLCD(s8 *str);
void U32LCD(u32 n);

#define UART0_TIMEOUT_LOOPS 2000000UL

typedef enum
{
    STATE_IDLE,
    STATE_WAIT_PIN,
    STATE_MENU
} State;

static State state = STATE_IDLE;
static char current_card[20] = "";
static u8 menu_page = 1;

/* Function declarations */
static void lcd_print(const char *msg);
static void show_page1(void);
static void show_page2(void);
static void show_menu(void);
static void exit_from_activity(void);
static void u32_to_str(u32 n, char *buf);
static void u32_to_padded_str(u32 n, char *buf, int width);
static int read_pin(char *pin_out, int width);
static int send_and_wait(const char *request,
                         char *out_buf, int out_size);
static void handle_idle(void);
static void handle_wait_pin(void);
static void handle_menu(void);
static void handle_pin_change(void);
static void show_result_and_wait(char *reply, char *txn_type, u32 txn_amount);
static void show_mini_statement(char *reply);
static void wait_for_activity_exit(void);

/* LCD string helper */
static void lcd_print(const char *msg)
{
    StrLCD((s8 *)msg);
}

/* First menu page */
static void show_page1(void)
{
    CmdLCD(CLEAR_LCD);
    lcd_print("1:Bal 2:Dep 3:Wd");

    CmdLCD(GOTO_LINE2_POS0);
    lcd_print("4:Next *:Exit");
}

/* Second menu page */
static void show_page2(void)
{
    CmdLCD(CLEAR_LCD);
    lcd_print("5:Mini 6:PIN");

    CmdLCD(GOTO_LINE2_POS0);
    lcd_print("4:Back *:Exit");
}

/* Display selected menu page */
static void show_menu(void)
{
    if (menu_page == 1)
        show_page1();
    else
        show_page2();
}

/* Return to authenticated menu */
static void exit_from_activity(void)
{
    CmdLCD(CLEAR_LCD);
    lcd_print("Going To Menu...");
    delay_ms(500);

    state = STATE_MENU;
}

/* Convert unsigned integer to decimal string */
static void u32_to_str(u32 n, char *buf)
{
    char temp[10];
    int i = 0;
    int j = 0;

    if (n == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (n > 0 && i < 10)
    {
        temp[i++] = (char)((n % 10) + '0');
        n /= 10;
    }

    while (i > 0)
    {
        buf[j++] = temp[--i];
    }

    buf[j] = '\0';
}

/* Convert number to fixed-width, zero-padded string */
static void u32_to_padded_str(u32 n, char *buf, int width)
{
    char raw[11];
    int raw_len;
    int pad;
    int i;

    u32_to_str(n, raw);

    raw_len = (int)strlen(raw);
    pad = width - raw_len;

    if (pad < 0)
        pad = 0;

    for (i = 0; i < pad; i++)
        buf[i] = '0';

    strcpy(buf + pad, raw);
}

/* Read number using the existing ReadNum() */
static int read_pin(char *pin_out, int width)
{
    s32 val;

    val = (s32)ReadNumMasked();

    if (val < 0)
        return 0;

    if (width == 4 && val > 9999)
        return 0;

    u32_to_padded_str((u32)val, pin_out, width);

    return 1;
}

/* Send request and wait for PC response */
static int send_and_wait(const char *request,
                         char *out_buf, int out_size)
{
    int len;

    UART0_SendString(request);

    len = UART0_ReceiveString(out_buf,
                              out_size,
                              UART0_TIMEOUT_LOOPS);

    if (len < 0)
    {
        CmdLCD(CLEAR_LCD);
        lcd_print("PC not responding");
        delay_ms(1500);
        return 0;
    }

    return 1;
}

/* Wait for exit key on an activity result screen */
static void wait_for_activity_exit(void)
{
    u32 key;
    CmdLCD(GOTO_LINE2_POS0);
    lcd_print("E:Menu *:Exit");
    while (1)
    {
        key = keyscan();
        if ((char)key == 'E')
        {
            menu_page = 1;
            state = STATE_MENU;
            show_menu();
            return;
        }
        else if ((char)key == '*')
        {
            current_card[0] = '\0';
            rfid_ready = 0;
            state = STATE_IDLE;
            CmdLCD(CLEAR_LCD);
            lcd_print("Tap Card...");
            return;
        }
    }
}

/* Display transaction response */
static void show_result_and_wait(char *reply, char *txn_type, u32 txn_amount)
{
    char *bal;

    CmdLCD(CLEAR_LCD);

    /* For balance inquiry, deposit, and withdrawal, show the resulting
       balance on line 1 without the protocol's @OK# prefix. */
    if ((strcmp(txn_type, "BALANCE") == 0 ||
         strcmp(txn_type, "DEPOSIT") == 0 ||
         strcmp(txn_type, "WITHDRAW") == 0) &&
        strstr(reply, "@OK#") != 0 && strstr(reply, "BALANCE:") != 0)
    {
        int i;

        bal = strstr(reply, "BALANCE:") + 8;
        CmdLCD(GOTO_LINE1_POS0);
        lcd_print("Bal:");
        for (i = 0; i < 12 && bal[i] != '\0' && bal[i] != '$' && bal[i] != '#' && bal[i] != '\r' && bal[i] != '\n'; i++)
            CharLCD(bal[i]);
    }
    else
    {
        /* Preserve error messages such as insufficient balance. */
        lcd_print(reply);
    }

    wait_for_activity_exit();
}

/* Display each mini-statement entry on LCD line 1, one at a time. */
static void show_mini_statement(char *reply)
{
    char *p;
    char *line;
    u32 key;
    p = strchr(reply, '\n');
    if (p != 0) p++;
    else p = reply;

    while (*p != '\0' && *p != '$')
    {
        char lcd_line[17];
        int i;
        line = p;
        while (*p != '\0' && *p != '\n' && *p != '$') p++;
        if (*p == '\n') { *p = '\0'; p++; }
        else if (*p == '$') *p = '\0';

        if (*line != '\0')
        {
            for (i = 0; line[i] != '\0' && i < 16; i++) lcd_line[i] = line[i];
            lcd_line[i] = '\0';
            CmdLCD(CLEAR_LCD);
            CmdLCD(GOTO_LINE1_POS0);
            lcd_print(lcd_line);
            delay_ms(1800);
        }
    }

    CmdLCD(CLEAR_LCD);
    lcd_print("Mini Statement");
    CmdLCD(GOTO_LINE2_POS0);
    lcd_print("E:Menu *:Exit");
    while (1)
    {
        key = keyscan();
        if ((char)key == 'E')
        {
            menu_page = 2;
            state = STATE_MENU;
            show_menu();
            return;
        }
        if ((char)key == '*')
        {
            current_card[0] = '\0';
            rfid_ready = 0;
            state = STATE_IDLE;
            CmdLCD(CLEAR_LCD);
            lcd_print("Tap Card...");
            return;
        }
    }
}

/* IDLE: wait for RFID card */
static void handle_idle(void)
{
    char reply[64];
    char request[32];

    if (!rfid_ready)
        return;

    strcpy(request, "#CARD:");
    strcat(request, (const char *)rfid_buffer);
    strcat(request, "$");

    strcpy(current_card, (const char *)rfid_buffer);

    rfid_ready = 0;

    if (!send_and_wait(request, reply, sizeof(reply)))
    {
        CmdLCD(CLEAR_LCD);
        lcd_print("Tap Card...");
        return;
    }

    if (strncmp(reply, "@OK", 3) == 0)
    {
        CmdLCD(CLEAR_LCD);
        lcd_print("Enter PIN:");
        state = STATE_WAIT_PIN;
    }
    else if (strstr(reply, "USER_BLOCKED") != 0)
    {
        CmdLCD(CLEAR_LCD);
        lcd_print("You are blocked");
        delay_ms(2000);
        CmdLCD(CLEAR_LCD);
        lcd_print("Tap Card...");
        state = STATE_IDLE;
    }
    else
    {
        CmdLCD(CLEAR_LCD);
        lcd_print("Invalid Card");
        delay_ms(1500);
        CmdLCD(CLEAR_LCD);
        lcd_print("Tap Card...");
        state = STATE_IDLE;
    }
}

/* PIN verification */
static void handle_wait_pin(void)
{
    char pin[10];
    char request[40];
    char reply[64];

    if (!read_pin(pin, 4))
    {
        CmdLCD(CLEAR_LCD);
        lcd_print("Cancelled");
        delay_ms(1000);

        CmdLCD(CLEAR_LCD);
        lcd_print("Tap Card...");

        state = STATE_IDLE;
        return;
    }

    strcpy(request, "#CARD:");
    strcat(request, current_card);
    strcat(request, "#PIN:");
    strcat(request, pin);
    strcat(request, "$");

    if (!send_and_wait(request, reply, sizeof(reply)))
    {
        state = STATE_IDLE;
        CmdLCD(CLEAR_LCD);
        lcd_print("Tap Card...");
        return;
    }

    if (strncmp(reply, "@OK", 3) == 0)
    {
        state = STATE_MENU;
        menu_page = 1;
        show_menu();
    }
    else if (strstr(reply, "USER_BLOCKED") != 0)
    {
        CmdLCD(CLEAR_LCD);
        lcd_print("You are blocked");
        delay_ms(2000);
        CmdLCD(CLEAR_LCD);
        lcd_print("Tap Card...");
        state = STATE_IDLE;
    }
    else
    {
        CmdLCD(CLEAR_LCD);
        lcd_print("Invalid PIN");
        delay_ms(1000);
        CmdLCD(CLEAR_LCD);
        lcd_print("Enter PIN:");
        state = STATE_WAIT_PIN;
    }
}

/* PIN change */
static void handle_pin_change(void)
{
    char new_pin[10];
    char confirm_pin[10];
    char request[64];
    char reply[64];

    CmdLCD(CLEAR_LCD);
    lcd_print("New PIN:");

    if (!read_pin(new_pin, 4))
    {
        show_menu();
        return;
    }

    CmdLCD(CLEAR_LCD);
    lcd_print("Confirm PIN:");

    if (!read_pin(confirm_pin, 4))
    {
        show_menu();
        return;
    }

    if (strcmp(new_pin, confirm_pin) != 0)
    {
        CmdLCD(CLEAR_LCD);
        lcd_print("PIN Mismatch");
        delay_ms(1500);

        show_menu();
        return;
    }

    /*
     * New PC-side protocol:
     * #PIN:CHANGE#NEW:1234#CARD:card_id$
     */
    strcpy(request, "#PIN:CHANGE#NEW:");
    strcat(request, new_pin);
    strcat(request, "#CARD:");
    strcat(request, current_card);
    strcat(request, "$");

    if (!send_and_wait(request, reply, sizeof(reply)))
    {
        state = STATE_IDLE;

        CmdLCD(CLEAR_LCD);
        lcd_print("Tap Card...");
        return;
    }

    CmdLCD(CLEAR_LCD);

    if (strncmp(reply, "@OK", 3) == 0)
        lcd_print("PIN Changed");
    else
        lcd_print(reply);

    wait_for_activity_exit();
}

/* Menu and transaction handling */
static void handle_menu(void)
{
    u32 key = keyscan();

    char request[64] = "";
    char reply[64];
    char amount_str[11];

    s32 amt;
    int is_mini_statement = 0;
    char txn_type[12] = "";
    u32 txn_amount = 0;

    switch ((char)key)
    {
        case '1':
            /* Balance inquiry */
            strcpy(txn_type, "BALANCE");
            strcpy(request, "#TXN:BALANCE#REQ$");
            break;

        case '2':
            strcpy(txn_type, "DEPOSIT");
            /* Deposit: accept positive amounts in multiples of 100, same as withdrawal. */
            while (1)
            {
                CmdLCD(CLEAR_LCD);
                lcd_print("amount:");

                amt = (s32)ReadNum();

                if (amt < 0)
                {
                    show_menu();
                    return;
                }

                if (amt == 0 || (amt % 100) != 0)
                {
                    CmdLCD(CLEAR_LCD);
                    lcd_print("Invalid Amount");
                    delay_ms(1500);
                    continue;
                }
                break;
            }

            txn_amount = (u32)amt;
            u32_to_str((u32)amt, amount_str);
            strcpy(request, "#TXN:DEPOSIT#AMOUNT:");
            strcat(request, amount_str);
            strcat(request, "$");
            break;

        case '3':
            strcpy(txn_type, "WITHDRAW");
            /*
             * Withdrawal: accept positive amounts in multiples of 100 only.
             * Invalid amounts are rejected locally and the user is prompted
             * again; no invalid withdrawal request is sent to Linux.
             */
            while (1)
            {
                CmdLCD(CLEAR_LCD);
                lcd_print("amount:");

                amt = (s32)ReadNum();

                if (amt < 0)
                {
                    show_menu();
                    return;
                }

                if (amt == 0 || (amt % 100) != 0)
                {
                    CmdLCD(CLEAR_LCD);
                    lcd_print("Invalid Amount");
                    delay_ms(1500);
                    continue;
                }

                break;
            }

            txn_amount = (u32)amt;
            u32_to_str((u32)amt, amount_str);

            strcpy(request, "#TXN:WITHDRAW#AMOUNT:");
            strcat(request, amount_str);
            strcat(request, "$");
            break;

        case '4':
            /* Next page / previous page */
            if (menu_page == 1)
                menu_page = 2;
            else
                menu_page = 1;

            show_menu();
            return;

        case '5':
            /* Mini statement */
            strcpy(request, "#TXN:MINISTAT#REQ$");
            is_mini_statement = 1;
            break;

        case '6':
            /* PIN change */
            handle_pin_change();
            return;

        case '*':
            /* Exit session */
            CmdLCD(CLEAR_LCD);
            lcd_print("EXITING...");
				    delay_ms(500);
           state = STATE_IDLE;
            break;

        default:
            return;
    }

    if (!send_and_wait(request, reply, sizeof(reply)))
    {
        state = STATE_IDLE;

        CmdLCD(CLEAR_LCD);
        lcd_print("Tap Card...");
        return;
    }

    if (is_mini_statement)
        show_mini_statement(reply);
    else
        show_result_and_wait(reply, txn_type, txn_amount);
}

/* Main */
int main(void)
{
    UART0_Init();
    UART1_Init();

    InitLCD();
    Init_KPM();

    CmdLCD(CLEAR_LCD);
    lcd_print("SecureBank Term.");
    delay_s(1);

    CmdLCD(CLEAR_LCD);
    lcd_print("Tap Card...");

    while (1)
    {
        switch (state)
        {
            case STATE_IDLE:
                handle_idle();
                break;

            case STATE_WAIT_PIN:
                handle_wait_pin();
                break;

            case STATE_MENU:
                handle_menu();
                break;

            default:
                state = STATE_IDLE;
                break;
        }
    }
}
