#include<lpc21xx.h>

delay_ms(unsigned int data)
{
data*=12000;
while(data--);
}
delay_us(unsigned int data)
{
data*=12;
while(data--);
}
delay_s(unsigned int data)
{
data*=12000000;
while(data--);
}
