// KPM.c - corrected while preserving the original 4x4 keypad logic
#include "types.h"
#include "define.h"
#include "delay.h"
#include "KPM_defines.h"
#include "lcd.h"
#include "lcd_defines.h"
#include <lpc21xx.h>

u8 KPMLUT[4][4] =
{
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','*'},
    {'C','0','=','E'}
};

void Init_KPM(void)
{
     WRITENIBBLE(IODIR1,ROW0,15);
}

u32 colscan(void)
{
    if(READNIBBLE(IOPIN1,COL0)<15)
                return 0;
        else
                return 1;
}

u32 colcheck(void)
{
    u32 cno;
        for(cno=0;cno<4;cno++)
        {
                if((READBIT(IOPIN1,(cno+COL0)))==0)
                        break;

        }
        return cno;
}

u32 rowcheck(void)
{
    u32 rno;
        for(rno=0;rno<4;rno++)
        {
                WRITENIBBLE(IOPIN1,ROW0,(~(1<<rno)));
                if(colscan()==0)
                        break;
        }
        IOCLR1=15<<ROW0;
        return rno;

}

u32 keyscan(void)
{
      u32 rno,cno,key;
        //wait for switch to press^M
        while(colscan());
        //find rno^M
        rno=rowcheck();
        //find cno^M
        cno=colcheck();
        key=KPMLUT[rno][cno];
        while(!colscan());
        delay_ms(400);
        return key;

}

static u32 ReadNumMode(u32 mask_digits)
{
    u8 key;
    u32 num=0,pos=0;

    while(1)
    {
        CmdLCD(DSP_ON_CUR_BLK);
        key=keyscan();

        if(key == 'E' && pos == 0)
        {
            CmdLCD(DSP_ON_CUR_OFF);
            return (u32)-1;
        }
        else if(key>='0'&&key<='9')
        {
            pos++;
            num=(num*10)+(key-48);
            if(mask_digits)
                CharLCD('*');
            else
                CharLCD(key);
        }
        else if(key =='B')
        {
            if(pos > 0)
            {
                num = num / 10;
                pos--;
                CmdLCD(0x10);   /* Move cursor left */
                CharLCD(' ');   /* Erase only last character */
                CmdLCD(0x10);   /* Return cursor to erased position */
            }
        }
        else if(key =='E')
            break;

        if(pos>6)
            break;
        delay_ms(500);
    }
    CmdLCD(DSP_ON_CUR_OFF);
    return num;
}

u32 ReadNum(void)
{
    return ReadNumMode(0); /* Amounts display their actual digits */
}

u32 ReadNumMasked(void)
{
    return ReadNumMode(1); /* PIN digits display as asterisks */
}
